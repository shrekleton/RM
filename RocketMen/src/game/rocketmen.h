
#pragma once

#include <core/game.h>

namespace rm
{
	class RocketMenGame : public Game
	{
	public:
		DECLARE_GAME_INFO("RocketMen", "0.0");

	public:
		bool initialize()                    override;
		void fixedUpdate(float deltaTime)    override;
		void update(const Time& time)        override;
		void terminate()                     override;
		void onPlayerJoin(int16_t playerId)  override;
	};

}; // namespace rm
