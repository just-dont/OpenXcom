/*
 * Copyright 2010-2014 OpenXcom Developers.
 *
 * This file is part of OpenXcom.
 *
 * OpenXcom is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * OpenXcom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with OpenXcom.  If not, see <http://www.gnu.org/licenses/>.
 */
#define _USE_MATH_DEFINES
#include <cmath>
#include "ItemSprite.h"
#include "../Engine/SurfaceSet.h"
#include "../Mod/RuleSoldier.h"
#include "../Mod/Unit.h"
#include "../Mod/RuleItem.h"
#include "../Mod/Armor.h"
#include "../Mod/RuleInventory.h"
#include "../Mod/Mod.h"
#include "../Savegame/BattleUnit.h"
#include "../Savegame/BattleItem.h"
#include "../Savegame/Soldier.h"
#include "../Battlescape/Position.h"

namespace OpenXcom
{

/**
 * Sets up a ItemSprite with the specified size and position.
 * @param width Width in pixels.
 * @param height Height in pixels.
 * @param x X position in pixels.
 * @param y Y position in pixels.
 */
ItemSprite::ItemSprite(Surface* dest, Mod* mod, int frame) :
	_itemSurface(mod->getSurfaceSet("FLOOROB.PCK")), _mod(mod),
	_animationFrame(frame),
	_dest(dest), _scriptWorkRef()
{

}

/**
 * Deletes the ItemSprite.
 */
ItemSprite::~ItemSprite()
{

}

/**
 * Draws a unit, using the drawing rules of the unit.
 * This function is called by Map, for each unit on the screen.
 */
void ItemSprite::draw(BattleItem* item, int x, int y, int shade, bool half)
{
	int sprite = item->getFloorSprite();
	if (sprite != -1)
	{
		BattleItem::ScriptFill(&_scriptWorkRef, item, false, _animationFrame, shade);
		_scriptWorkRef.executeBlit(_itemSurface->getFrame(sprite), _dest, x, y, half);
	}
}

} //namespace OpenXcom