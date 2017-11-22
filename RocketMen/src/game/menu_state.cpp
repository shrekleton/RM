
#include "menu_state.h"

#include <common.h>
#include <core/input.h>
#include <game/game_state_ids.h>
#include <game/rocketmen.h>
#include <network/network.h>
#include <utility/commandline_options.h>

using namespace rm;

static const network::Address localhost("127.0.0.1", s_defaultServerPort);
//=============================================================================

MenuState::MenuState()
{
}

MenuState::~MenuState()
{
}

void MenuState::initialize(Game* game)
{
	m_locked = false;
	m_game = game;
}

void MenuState::enter(Game* /*game*/)
{

}

void MenuState::destroy(Game* /*game*/)
{
}

void MenuState::update(Game* game, const Time& /*time*/)
{
	assert(game == m_game);
	if (m_locked)
	{
		return;
	}

	if (input::getKey(input::Key::NUM_1))
	{
		hostAndJoin(game);
	}
	else if (input::getKey(input::Key::NUM_2))
	{
		hostDedicated(game);
	}
	else if (input::getKey(input::Key::NUM_3))
	{
		join(game);
	}
}

void MenuState::tick(Game* /*game*/, float /*fixedDeltaTime*/)
{
}

void MenuState::render(Game* /*game*/)
{
}

void MenuState::parseCommandLineOptions(const CommandLineOptions& options)
{
	if (options.isSet("--listen"))
	{
		hostAndJoin(m_game);
	}
	else if (options.isSet("--dedicated"))
	{
		hostDedicated(m_game);
	}
}

void MenuState::hostAndJoin(Game* game)
{
	assert(game == m_game);

	m_locked = true;
	if (game->createSession(GameSessionType::Online))
	{
		game->joinSession(localhost, std::bind(&MenuState::onResult, this, std::placeholders::_1));
	}
}

void MenuState::hostDedicated(Game* game)
{
	assert(game == m_game);
	m_locked = true;
	if (game->createSession(GameSessionType::Online))
	{
		game->pushState(GameStateID::Gameplay);
	}
}

void MenuState::join(Game* game)
{
	m_locked = true;
	game->joinSession(localhost, std::bind(&MenuState::onResult, this, std::placeholders::_1));
}

void MenuState::onResult(SessionResult result)
{
	if (result == SessionResult::Joined)
	{
		m_game->pushState(GameStateID::Gameplay);
	}
	m_locked = false;
}
