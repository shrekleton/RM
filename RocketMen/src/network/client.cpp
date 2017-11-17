
#include <network/client.h>

#include <core/action_buffer.h>
#include <core/entity.h>
#include <core/entity_manager.h>
#include <core/game.h>
#include <core/input.h>
#include <core/debug.h>
#include <game_time.h>
#include <network.h>
#include <network/address.h>
#include <network/server.h>
#include <network/socket.h>
#include <utility.h>

extern "C" unsigned long crcFast(unsigned char const message[], int nBytes);

using namespace network;

static const int16_t s_firstTempNetworkId = -2; // Reserve -1 for INDEX_NONE
static int32_t s_nextTempNetworkId = s_firstTempNetworkId;

static ActionBuffer s_playerActions[s_maxPlayersPerClient];

//=============================================================================

Client::Client(Time& time, Game* game) :
	m_gameTime(time),
	m_game(game),
	m_connection(nullptr),
	m_lastReceivedState((Sequence)INDEX_NONE),
	m_lastFrameSent(0),
	m_lastFrameSimulated(0),
	m_lastOrderedMessaged(0),
	m_state(State::Disconnected),
	m_stateTimer(0.0f),
	m_timeSinceLastInputMessage(0.0f),
	m_maxInputMessageSentTime(0.05f),
	m_timeSinceLastClockSync(0.f),
	m_clockResyncTime(5.f),
	m_isInitialized(false),
	m_localPlayers(s_maxPlayersPerClient)

{
	clearSession();
	m_socket = Socket::create();
	assert(m_socket != nullptr);
}

Client::~Client()
{
	delete m_socket;
}

void Client::initialize(uint16_t port)
{
	clearSession();

	m_port = port;
	m_isInitialized = true;
}

bool Client::isInitialized() const
{
	return m_isInitialized;
}

void Client::update()
{
	const float deltaTime = m_gameTime.getDeltaSeconds();

	if (m_state != State::Disconnected)
	{
		assert(m_connection != nullptr);
		m_timeSinceLastInputMessage += deltaTime;

		receivePackets();
		readMessages();
		readInput();

		if (m_state == State::Connected)
		{
			if (m_timeSinceLastInputMessage >= m_maxInputMessageSentTime)
			{
				m_timeSinceLastInputMessage = 0.f;
				sendPlayerActions();
				for (LocalPlayer player : m_localPlayers)
				{
					syncOwnedEntities(player.playerId);
				}
			}

			if (m_timeSinceLastClockSync > m_clockResyncTime)
			{
				requestServerTime();
			}		
		}

		sendPendingMessages();
		
		if (!Network::isServer())
		{
			m_connection->update(m_gameTime);
		}
	}

	m_stateTimer += deltaTime;
}

void Client::sendPlayerActions()
{
	if (sequenceLessThan(m_lastFrameSimulated, m_lastFrameSent)
		|| Network::isServer())
	{
		return;
	}

	if (Frame* frame = m_clientHistory.getFrame(m_lastFrameSent))
	{
		const int16_t numFramesToSend = m_lastFrameSimulated - m_lastFrameSent;
		
		for (int16_t i = 0; i < numFramesToSend; i++)
		{
			const int16_t frameId = m_lastFrameSent + i;
			for (LocalPlayer& player : m_localPlayers)
			{
				Message message = {};
				message.type = MessageType::PlayerInput;
				message.data.writeInt16(player.playerId);
				message.data.writeInt16(frameId);
				frame->actions[player.playerId].writeToMessage(message);
				frame->actions[player.playerId].clear();
				sendMessage(message);
			}

			m_lastFrameSent = frameId;
			frame++;
		}
	}
}

void Client::requestServerTime()
{
	OutgoingMessage pingMessage = {};
	pingMessage.type = MessageType::ClockSync;
	pingMessage.data.writeInt64(m_gameTime.getMilliSeconds());
	sendMessage(pingMessage);
}

void Client::readInput()
{
	for (const LocalPlayer& player : m_localPlayers)
	{
		ActionBuffer& inputActions = s_playerActions[player.playerId];
		input::getActions(player.controllerId, inputActions, player.listenMouseKB);
	}
}

void Client::simulate(Sequence frameId)
{
	Frame* currentFrame = m_clientHistory.insertFrame(frameId);

	if (m_state == Client::State::Connected)
	{
		for (LocalPlayer& player : m_localPlayers)
		{
			ActionBuffer& playerActions = s_playerActions[player.playerId];
			if (!playerActions.isEmpty())
			{
				currentFrame->actions[player.playerId].insert(playerActions);
				m_game->processPlayerActions(playerActions, player.playerId);
				playerActions.clear();
			}
		}
	}
	m_lastFrameSimulated = frameId;
}

void Client::connect(const Address& address)
{
	if (!ensure(m_state == Client::State::Disconnected))
	{
		LOG_ERROR("Client::connect: Already connected");
		return;
	}

	assert(m_connection == nullptr);
	assert(m_socket->isInitialized() == false);

	if (m_socket->initialize(m_port))
	{
		m_connection = new Connection(m_socket, address, 
			std::bind(&Client::onConnectionCallback, this, std::placeholders::_1, std::placeholders::_2));

		m_connection->tryConnect();
		m_state = Client::State::Connecting;		
	}
	else
	{
		LOG_ERROR("Client: Failed to initialize socket");
	}
}

void Client::disconnect()
{
	if (m_state != State::Connected || m_state != State::Connecting)
	{
		ensure(false);
		return;
	}

	setState(State::Disconnecting);
	m_connection->close();
}

LocalPlayer& Client::addLocalPlayer(int32_t controllerId, bool enableMouseKB)
{
	assert(m_localPlayers.getCount() < s_maxPlayersPerClient);

	LocalPlayer& player = m_localPlayers.insert();
	player.controllerId = controllerId;
	player.listenMouseKB = enableMouseKB;
	return player;
}

bool Client::requestEntity(Entity* entity)
{
	assert(entity != nullptr);
	assert(entity->getNetworkId() == INDEX_NONE);

	const int32_t tempId = getNextTempNetworkId();
	entity->setNetworkId(tempId);

	Message message = {};
	message.type = MessageType::RequestEntity;
	message.data.writeInt32(tempId);

	WriteStream stream(32);
	EntityManager::serializeFullEntity(entity, stream);
	message.data.writeFromStream(stream);
	
	m_connection->sendMessage(message);
	LOG_DEBUG("Client::requestEntity temp_%d", tempId);
	return true;
}

void Client::requestEntity(int32_t netId)
{
	assert(netId > INDEX_NONE && netId < s_maxNetworkedEntities);
	if (!m_requestedEntities.contains(netId))
	{
		m_requestedEntities.insert(netId);
		LOG_DEBUG("Client::requestEntity %d", netId);
		Message message = {};
		message.type = MessageType::RequestEntity;
		message.data.writeInt32(netId);
		m_connection->sendMessage(message);
	}
}

uint32_t Client::getNumLocalPlayers() const
{
	return m_localPlayers.getCount();
}

bool Client::isLocalPlayer(int16_t playerId) const
{
	return getLocalPlayer(playerId) != nullptr;
}

LocalPlayer* Client::getLocalPlayer(int16_t playerId) const
{
	for (auto& localPlayer : m_localPlayers)
	{
		if (localPlayer.playerId == playerId)
		{
			return &localPlayer;
		}
	}

	return nullptr;
}

void Client::syncOwnedEntities(int16_t playerId)
{
	std::vector<Entity*> entities = EntityManager::getEntities();
	std::vector<Entity*> ownedEntities;
	std::copy_if(entities.begin(), entities.end(), std::back_inserter(ownedEntities), [playerId](Entity* entity)
	{
		return entity->getOwnerPlayerId() == playerId;
	});
	
	int32_t entityCount = static_cast<int32_t>(ownedEntities.size());

	if (entityCount > 0)
	{
		Message message = {};
		message.type = MessageType::Gamestate;
		WriteStream stream(32);
		serializeInt(stream, entityCount);
		for (Entity* entity : ownedEntities)
		{
			int32_t networkId = entity->getNetworkId();
			serializeInt(stream, networkId);
			EntityManager::serializeClientVars(entity, stream);
		}
		message.data.writeFromStream(stream);
		sendMessage(message);
	}
}

void Client::readMessage(IncomingMessage& message)
{
	switch (message.type)
	{
		case MessageType::Gamestate:
		{
			if (sequenceLessThan(message.sequence, m_lastReceivedState))
			{ // old, discard
				break;
			}
			
			m_lastReceivedState = message.sequence;
			onGameState(message);
			break;
		}
		case MessageType::SpawnEntity:
		{
			onSpawnEntity(message);
			break;
		}
		case MessageType::AcceptEntity:
		{
			onAcceptEntity(message);
			break;
		}
		case MessageType::DestroyEntity:
		{
			onDestroyEntity(message);
			break;
		}

		case MessageType::AcceptClient:
		{
			onConnectionEstablished(message);
			break;
		}
		case MessageType::AcceptPlayer:
		{
			onAcceptPlayer(message);
			break;
		}
		case MessageType::ClockSync:
		{
			onReceiveServerTime(message);
			break;
		}
		case MessageType::KeepAlive:
		case MessageType::RequestEntity:
		case MessageType::IntroducePlayer:
		case MessageType::PlayerInput:
		case MessageType::None:
		case MessageType::RequestConnection:
		case MessageType::Disconnect:
		case MessageType::GameEvent:
		case MessageType::NUM_MESSAGE_TYPES:
		{
			break;
		}
	}
}

void Client::onConnectionEstablished(IncomingMessage& msg)
{
	if (!ensure(m_state == State::Connecting))
		return;

	int32_t id = msg.data.readInt32();
	LOG_INFO("Client: Connection established with the server, Client ID: %d", id);
	setState(State::Connected);

	m_lastFrameSent = m_lastFrameSimulated;

	if (Network::isServer())
	{
		Network::getLocalServer()->registerLocalClientId(id);
	}

	// Introduce Players
	Message outMessage = {};
	outMessage.type = MessageType::IntroducePlayer;
	outMessage.data.writeInt32(getNumLocalPlayers());

	sendMessage(outMessage);
}

void Client::onAcceptPlayer(IncomingMessage& msg)
{
	if (m_localPlayers[0].playerId != INDEX_NONE)
	{
		assert(false);
		return;
	}
	
	for (auto& player : m_localPlayers)
	{
		player.playerId = msg.data.readInt16();
		LOG_INFO("Received player id %i", player.playerId);
	}
}

void Client::onSpawnEntity(IncomingMessage& msg)
{
	ReadStream readStream(static_cast<int32_t>(msg.data.getLength()));
	msg.data.readToStream(readStream);
	int32_t networkId = INDEX_NONE;
	serializeInt(readStream, networkId);
	if (networkId > INDEX_NONE)
	{
		int32_t index = m_requestedEntities.find(networkId);
		if (index != INDEX_NONE)
		{
			m_requestedEntities[index] = INDEX_NONE;
		}

		auto& entities = EntityManager::getEntities();
		if (Entity* entity = findPtrByPredicate(entities.begin(), entities.end(),
			[networkId](Entity* it) { return it->getNetworkId() == networkId; }))
		{
			return;
		}

#ifdef _DEBUG
		Entity* entity = EntityManager::instantiateEntity(readStream);
		LOG_DEBUG("Client: Spawned entity ID: %d netID: %d", entity->getId(), entity->getNetworkId());
#else
		EntityManager::instantiateEntity(readStream);
#endif // _DEBUG
	}
}

void Client::onAcceptEntity(IncomingMessage& msg)
{
	int32_t localId  = msg.data.readInt32();
	int32_t remoteId = msg.data.readInt32();

	int32_t index = m_requestedEntities.find(localId);
	if (index != INDEX_NONE)
	{
		m_requestedEntities[index] = INDEX_NONE;
	}

	auto& entities = EntityManager::getEntities();
	if (Entity* entity = findPtrByPredicate(entities.begin(), entities.end(),
		[localId](Entity* it) { return it->getNetworkId() == localId; } ))
	{
		entity->setNetworkId(remoteId);
#ifdef _DEBUG
		LOG_DEBUG("Client: Accepted entity ID: %d netID: %d", entity->getId(), entity->getNetworkId());
#endif
	}
	else
	{
		LOG_DEBUG("Client::onAcceptEntity: Unknown netID (%i)", localId);
	}
}

void Client::onDestroyEntity(IncomingMessage& msg)
{
	int32_t networkId = msg.data.readInt32();

	if (networkId < -s_maxSpawnPredictedEntities
		|| networkId > s_maxNetworkedEntities)
	{
		return;
	}

	auto entityList = EntityManager::getEntities();
	if (Entity* netEntity = findPtrByPredicate(entityList.begin(), entityList.end(),
		[networkId](Entity* entity) -> bool { return entity->getNetworkId() == networkId; }))
	{
		netEntity->kill();
	}
} 

void Client::onGameState(IncomingMessage& msg)
{
	std::vector<Entity*>& entities = EntityManager::getEntities();
	ReadStream readStream(s_maxSnapshotSize);

	msg.data.readBytes((char*)readStream.getBuffer(),
        msg.data.getLength() - msg.data.getReadTotalBytes());

	int32_t numReceivedEntities = 0;
	serializeInt(readStream, numReceivedEntities);
	if (numReceivedEntities <= 0 || numReceivedEntities > s_maxNetworkedEntities)
		return;

	int32_t numReadEntities = 0;

	for (int32_t networkId = 0; networkId < s_maxNetworkedEntities; networkId++)
	{
		bool readEntity = false;
		serializeBit(readStream, readEntity);
		if (readEntity)
		{
			numReadEntities++;

			if (Entity* netEntity = findPtrByPredicate(entities.begin(), entities.end(),
				[networkId](Entity* entity) -> bool { return entity->getNetworkId() == networkId; }))
			{
				EntityManager::serializeEntity(netEntity, readStream);
			}
			else
			{
				requestEntity(networkId);
			}
		}
		if (numReadEntities >= numReceivedEntities)
		{
			break;
		}
	}	
}

void Client::onReceiveServerTime(IncomingMessage& message)
{
	const uint64_t pingSentTime    = message.data.readInt64();
	const uint64_t pongReceiveTime = message.data.readInt64();
	const uint64_t currentTime     = m_gameTime.getMilliSeconds();
	const uint64_t latency         = currentTime - pingSentTime;

	// TODO resync game clock
	// http://www.mine-control.com/zack/timesync/timesync.html
}

void Client::sendMessage(Message& message)
{
	assert(m_connection != nullptr);
	m_connection->sendMessage(message);
}

void Client::sendPendingMessages()
{
	assert(m_connection != nullptr);

	m_connection->sendPendingMessages(m_gameTime);
}

void Client::setState(State state)
{
	m_state      = state;
	m_stateTimer = 0.0f;
}

void Client::clearSession()
{
	m_localPlayers.clear();
	m_recentlyProcessed.fill(INDEX_NONE);
	m_recentlyDestroyedEntities.fill(INDEX_NONE);
	m_requestedEntities.fill(INDEX_NONE);
	
	if (m_connection)
	{
		delete m_connection;
	}
}

int32_t Client::getNextTempNetworkId()
{
	int32_t tempNetworkId = s_nextTempNetworkId--;
	if (s_nextTempNetworkId <= -s_maxSpawnPredictedEntities - 1)
	{
		s_nextTempNetworkId = s_firstTempNetworkId;
	}

	return tempNetworkId;
}

void Client::receivePackets()
{
	assert(m_socket != nullptr);
	assert(m_socket->isInitialized());

	Address address;
	char    buffer[g_maxPacketSize];
	int32_t length = 0;

	while (m_socket->receive(address, buffer, length))
	{
		assert(length <= g_maxPacketSize);

		// Reconstruct packet
		BitStream stream;
		stream.writeBuffer(buffer, length);

		const uint32_t checksum = stream.readInt32();

		PacketHeader packetHeader;
		stream.readBytes((char*)&packetHeader, sizeof(PacketHeader));

		if (packetHeader.dataLength < g_maxBlockSize)
		{
			ChannelType channel = (packetHeader.sequence == (Sequence)-1
				&& packetHeader.ackBits == (uint32_t)-1
				&& packetHeader.ackSequence == (Sequence)-1) ?
				ChannelType::Unreliable :
				ChannelType::ReliableOrdered;

			Packet packet(channel);
			packet.header = packetHeader;
			stream.readBytes(packet.getData(), packet.header.dataLength);

			// Write protocol ID after packet to include in the checksum
			memcpy(packet.getData() + packet.header.dataLength, &g_protocolId, sizeof(g_protocolId));

			if (checksum == crcFast((const unsigned char*)packet.getData(),
				packet.header.dataLength + sizeof(uint32_t)))
			{
				if (address == m_connection->getAddress())
				{
					m_connection->receivePacket(packet);
				}
			}
			else
			{
				LOG_DEBUG("PacketReceiver::receivePackets: Checksum mismatched, packet discarded.");
			}
		}
	}
}
void Client::readMessages()
{
	while (IncomingMessage* message = m_connection->getNextMessage())
	{
		readMessage(*message);
		message->type = MessageType::None;
	}
}

void Client::onConnectionCallback(ConnectionCallback type, Connection* connection)
{
	assert(connection != nullptr);
	assert(connection == m_connection);

	switch (type)
	{
		case ConnectionCallback::ConnectionEstablished:
		{
			setState(State::Connected);
			break;
		}
		case ConnectionCallback::ConnectionFailed:
		case ConnectionCallback::ConnectionLost:
		{
			LOG_INFO(type == ConnectionCallback::ConnectionLost ? 
				"Client: Lost connection to the server" : "Client: Failed to connect to the server");
			disconnect();
			break;
		}
		case ConnectionCallback::ConnectionReceived:
		{
			assert(false);
			break;
		}
	}
}
