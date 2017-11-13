
#pragma once

#include <buffer.h>
#include <circular_buffer.h>
#include <core/entity.h>
#include <network/common_network.h>
#include <network/connection.h>
#include <network/connection_callback.h>
#include <network/network_message.h>

#include <array>
#include <cstdint>

class Game;
class Time;
class ActionBuffer;

//=============================================================================

namespace network 
{
	struct LocalPlayer
	{
		LocalPlayer() : playerId(INDEX_NONE), controllerId(INDEX_NONE) {}

		int32_t playerId;
		int32_t controllerId;
	};

	class Client
	{
	private:
		static const uint32_t s_sequenceMemorySize    = 256;
		static const uint32_t s_recentlyDestroyedSize = 32;

	public:
		Client(Time& time, Game* game);
		~Client();

		enum class State
		{
			Disconnected,
			Connecting,
			Connected,
			Disconnecting
		};

	public:
		bool initialize(uint16_t port);
		bool isInitialized() const;
		void update();

		void readInput();
		void requestServerTime();
		void fixedUpdate();
		void connect(const Address& address);
		void disconnect();
		LocalPlayer& addLocalPlayer(int32_t controllerId);
		void clearLocalPlayers();
		bool requestEntity(Entity* entity);

		uint32_t getNumLocalPlayers() const;
		bool isLocalPlayer(int32_t playerId) const;
		LocalPlayer* getLocalPlayer(int32_t playerId) const;

	private:
		void readMessage(IncomingMessage& message);
		void onConnectionEstablished(IncomingMessage& message);
		void onAcceptPlayer(IncomingMessage& message);
		void onSpawnEntity(IncomingMessage& message);
		void onAcceptEntity(IncomingMessage& message);
		void onDestroyEntity(IncomingMessage& message);
		void onGameState(IncomingMessage& message);
		void onReceiveServerTime(IncomingMessage& message);

		void sendMessage(Message& message);
		void sendPendingMessages();
		void setState(State state);
		void clearSession();
		int16_t getNextTempNetworkId();

		void receivePackets();
		void readMessages();
		void onConnectionCallback(ConnectionCallback type, Connection* connection);

		Socket*     m_socket;
		Game*       m_game;
		Connection* m_connection;
		Time&       m_gameTime;
		Sequence    m_lastReceivedState;
		uint32_t    m_lastOrderedMessaged;
		State       m_state;
		float       m_stateTimer;
		float       m_messageSentTime;
		float       m_maxMessageSentTime;
		float       m_timeSinceLastClockSync;
		bool        m_isInitialized;
		uint16_t    m_port;

		CircularBuffer<int32_t, s_sequenceMemorySize> 
			m_recentlyProcessed;

		CircularBuffer<int32_t, s_recentlyDestroyedSize>
			m_recentlyDestroyedEntities;

		CircularBuffer<int32_t, s_maxSpawnPredictedEntities>
			m_recentlyPredictedSpawns;

		Buffer<LocalPlayer>	m_localPlayers;	
	};
}; //namespace network