
#pragma once

#include "address.h"
#include "bitstream.h"

namespace network
{
	static const int32_t s_maxPendingMessages = 64;

	enum class MessageType : int8_t
	{
		MESSAGE_CLEAR = 0,
		MESSAGE_ACK,

		// Client to server
		CLIENT_CONNECT_REQUEST,
		CLIENT_DISCONNECT,

		// Server to client
		CLIENT_CONNECT_ACCEPT,
		SERVER_GAMESTATE,

		// Client to server
		PLAYER_INTRO,
		PLAYER_INPUT,

		NUM_COMMANDS
	};

	struct NetworkMessage
	{
		MessageType type;
		BitStream*  data;
		bool        isReliable;
		bool        isOrdered;
		int32_t     sequenceNr;
	};

	struct IncomingMessage
	{
		MessageType type;
		BitStream*  data;
		bool        isReliable;
		bool        isOrdered;
		Address     address; // sender address
		int32_t     sequenceNr;
	};

	inline void destroyMessage(NetworkMessage& message)
	{
		if (message.data != nullptr)
		{
			delete message.data;
		}
	}

	inline void destroyMessage(IncomingMessage& message)
	{
		if (message.data != nullptr)
		{
			delete message.data;
		}
	}

}; // namespace network