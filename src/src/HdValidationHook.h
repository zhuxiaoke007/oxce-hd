/*
 * Temporary HD-font validation hook (P5 in-game screenshot automation).
 * NOT part of the shipped feature; guarded by __HDFONTS__ and only active
 * when launched with `-hdshot:<savefile>` (e.g. -hdshot:2.sav).
 *
 * Flow: wait a few ticks, then load the save exactly like LoadGameState does
 * and hand control to a normally-running GeoscapeState/BattlescapeState.
 * Screenshots are taken externally via the F12 key (proven path); the
 * legacy comparison is produced by a second run with hdFonts:false.
 */
#pragma once

#ifdef __HDFONTS__

#include "Engine/State.h"
#include "Engine/Options.h"
#include "Engine/Screen.h"
#include "Engine/Logger.h"
#include "Engine/Exception.h"
#include "Savegame/SavedGame.h"
#include "Savegame/SavedBattleGame.h"
#include "Geoscape/GeoscapeState.h"
#include "Battlescape/BattlescapeState.h"
#include "Menu/OptionsAdvancedState.h"
#include "Menu/OptionsVideoState.h"
#include "Ufopaedia/UfopaediaStartState.h"
#include "Ufopaedia/UfopaediaSelectState.h"
#include "Basescape/BasescapeState.h"
#include "Basescape/SelectStartFacilityState.h"
#include "Savegame/Base.h"
#include "Savegame/BaseFacility.h"
#include "Battlescape/ActionMenuState.h"
#include "Battlescape/BattlescapeGame.h"
#include "Savegame/BattleUnit.h"
#include "Savegame/BattleItem.h"

namespace OpenXcom
{

/// Waits until the freshly entered battlescape has fully initialized (map
/// cache, animation state), then pops the action menu exactly like a real
/// weapon click would — pushing it too early renders into an unset palette.
class HdDelayedMenu : public State
{
	int _t;
public:
	HdDelayedMenu() : _t(0) {}
	void think() override
	{
		if (++_t < 1500) // ~25 s at 60 fps
			return;
		_game->popState();
		BattleAction *act = new BattleAction();
		for (auto *u : *_game->getSavedGame()->getSavedBattle()->getUnits())
		{
			if (u->getFaction() != FACTION_PLAYER) continue;
			for (auto *it : *u->getInventory())
			{
				if (it->getRules()->getBattleType() == BT_FIREARM)
				{
					act->actor = u;
					act->weapon = it;
					break;
				}
			}
			if (act->weapon != 0) break;
		}
		if (act->weapon != 0)
		{
			_game->pushState(new ActionMenuState(act, 24, 120));
			Log(LOG_INFO) << "HdShot: delayed-pushed ActionMenuState";
		}
		else
		{
			delete act;
			Log(LOG_INFO) << "HdShot: no firearm found";
		}
	}
};

/// Auto-loads a save after startup, then hands control to the loaded scene.
class HdShotLoader : public State
{
private:
	int _ticks;
	std::string _save;
	bool _loaded;
public:
	HdShotLoader(const std::string &save) : _ticks(0), _save(save), _loaded(false)
	{
	}
	void think() override
	{
		if (_loaded)
			return;
		if (++_ticks < 5)
			return;
		_loaded = true;

		_game->popState(); // remove this loader

		SavedGame *s = new SavedGame();
		try
		{
			Log(LOG_INFO) << "HdShot: loading " << _save;
			s->load(_save, _game->getMod(), _game->getLanguage());
			Log(LOG_INFO) << "HdShot: loaded ok";
			_game->setSavedGame(s);
			if (_game->getSavedGame()->getEnding() != END_NONE)
			{
				Log(LOG_ERROR) << "HdShot: save has an ending, cannot validate";
				_game->quit();
				return;
			}
			Options::baseXResolution = Options::baseXGeoscape;
			Options::baseYResolution = Options::baseYGeoscape;
			_game->getScreen()->resetDisplay(false);
			GeoscapeState *geo = new GeoscapeState;
			_game->setState(geo);
			if (_game->getSavedGame()->getSavedBattle() != 0)
			{
				_game->getSavedGame()->getSavedBattle()->loadMapResources(_game->getMod());
				Options::baseXResolution = Options::baseXBattlescape;
				Options::baseYResolution = Options::baseYBattlescape;
				_game->getScreen()->resetDisplay(false);
				BattlescapeState *bs = new BattlescapeState;
				_game->pushState(bs);
				_game->getSavedGame()->getSavedBattle()->setBattleState(bs);
			}
			Log(LOG_INFO) << "HdShot: scene entered, waiting for external F12";
			// Verification extension: also open the Advanced Options screen,
			// where embedded TextList rows are reported invisible with HD fonts.
			if (_save.rfind("opt", 0) == 0)
			{
				_game->pushState(new OptionsAdvancedState(OPT_GEOSCAPE));
				Log(LOG_INFO) << "HdShot: pushed OptionsAdvancedState";
			}
			// ComboBox verification: the video page has four combo boxes
			// (display mode / language / globe size / battle size) whose
			// button labels are reported blank.
			if (_save.rfind("optv", 0) == 0)
			{
				_game->pushState(new OptionsVideoState(OPT_GEOSCAPE));
				Log(LOG_INFO) << "HdShot: pushed OptionsVideoState";
			}
			// Bug3 scenario: battlescape action menu (fire-mode popup) — the
			// ActionMenuItem boxes are reported blank. Needs a battle save;
			// pushed after a delay so the battle is fully initialized.
			if (_save.rfind("actm", 0) == 0 && _game->getSavedGame()->getSavedBattle() != 0)
			{
				_game->pushState(new HdDelayedMenu);
				Log(LOG_INFO) << "HdShot: armed delayed ActionMenuState";
			}
			// Bug2 scenario (user-requested flow): geoscape -> pick spot ->
			// name the base -> place access lift -> facility-pick page, i.e.
			// the stack Basescape + SelectStartFacilityState for a freshly
			// created base (mimics BaseNameState/PlaceLiftState completion).
			if (_save.rfind("newb", 0) == 0)
			{
				Base *base0 = _game->getSavedGame()->getBases()->front();
				Base *b = new Base(_game->getMod());
				b->setLongitude(base0->getLongitude());
				b->setLatitude(base0->getLatitude());
				b->setName("HD Test Base");
				for (const auto &facilityType : _game->getMod()->getBaseFacilitiesList())
				{
					auto *rule = _game->getMod()->getBaseFacility(facilityType);
					if (rule->isLift() && !rule->isUpgradeOnly())
					{
						BaseFacility *fac = new BaseFacility(rule, b);
						fac->setX(4);
						fac->setY(4);
						b->getFacilities()->push_back(fac);
						break;
					}
				}
				_game->getSavedGame()->getBases()->push_back(b);
				_game->getSavedGame()->setSelectedBase(_game->getSavedGame()->getBases()->size() - 1);
				BasescapeState *bs = new BasescapeState(b, geo->getGlobe());
				_game->pushState(bs);
				_game->pushState(new SelectStartFacilityState(b, bs, geo->getGlobe()));
				Log(LOG_INFO) << "HdShot: pushed new-base flow (Basescape + SelectStartFacilityState)";
			}
			// Bug2 scenario: initial-base construction. Replicates the stack
			// Geoscape <- Basescape <- SelectStartFacilityState (the facility
			// pick list on the right side is reported blank).
			if (_save.rfind("selb", 0) == 0)
			{
				Base *b = _game->getSavedGame()->getBases()->front();
				BasescapeState *bs = new BasescapeState(b, geo->getGlobe());
				_game->pushState(bs);
				_game->pushState(new SelectStartFacilityState(b, bs, geo->getGlobe()));
				Log(LOG_INFO) << "HdShot: pushed Basescape + SelectStartFacilityState";
			}
			// Layering regression scenario: Ufopaedia section list (popup on
			// top of the start state) — text of the state below must NOT
			// bleed through the popup window via the HD overlay.
			if (_save.rfind("ufop", 0) == 0)
			{
				const auto &cats = _game->getMod()->getUfopaediaCategoryList();
				std::string firstCat = cats.empty() ? std::string() : cats.front();
				_game->pushState(new UfopaediaStartState);
				_game->pushState(new UfopaediaSelectState(firstCat, 0, 0));
				Log(LOG_INFO) << "HdShot: pushed UfopaediaStartState + UfopaediaSelectState('" << firstCat << "')";
			}
			// Clear the SDL event queue (ignore input from impatient users)
			SDL_Event e;
			while (SDL_PollEvent(&e))
			{
				// do nothing
			}
		}
		catch (Exception &e)
		{
			Log(LOG_ERROR) << "HdShot: load failed: " << e.what();
			_game->quit();
		}
		catch (...)
		{
			Log(LOG_ERROR) << "HdShot: unknown load error";
			_game->quit();
		}
	}
};

} // namespace OpenXcom

#endif // __HDFONTS__
