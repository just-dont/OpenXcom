#pragma once
/*
 * Copyright 2010-2016 OpenXcom Developers.
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
#include <string>
#include <map>
#include <yaml-cpp/yaml.h>

namespace OpenXcom
{

enum ManufacturingFilterType
{
	MANU_FILTER_DEFAULT,
	MANU_FILTER_DEFAULT_SUPPLIES_OK,
	MANU_FILTER_DEFAULT_NO_SUPPLIES,
	MANU_FILTER_FACILITY_REQUIRED,
	MANU_FILTER_HIDDEN
};
	
class RuleResearch;
class RuleItem;
class RuleCraft;
class Mod;

/**
 * Represents the information needed to manufacture an object.
 */
class RuleManufacture
{
private:
	std::string _name, _category;
	std::string _spawnedPersonType, _spawnedPersonName;
	std::vector<std::string> _requiresName, _requiresBaseFunc;
	std::vector<const RuleResearch*> _requires;
	int _space, _time, _cost;
	bool _refund;
	std::map<std::string, int> _requiredItemsNames, _producedItemsNames;
	std::map<const RuleItem*, int> _requiredItems, _producedItems;
	std::map<const RuleCraft*, int> _requiredCrafts;
	const RuleCraft* _producedCraft;
	int _listOrder;
public:
	static const int MANU_STATUS_NEW = 0;
	static const int MANU_STATUS_NORMAL = 1;
	static const int MANU_STATUS_HIDDEN = 2;
	static const int MANU_STATUSES = 3;
	/// Creates a new manufacture.
	RuleManufacture(const std::string &name);

	/// Loads the manufacture from YAML.
	void load(const YAML::Node& node, int listOrder);
	/// Cross link with other rules.
	void afterLoad(const Mod* mod);

	/// Gets the manufacture name.
	const std::string &getName() const;
	/// Gets the manufacture category.
	const std::string &getCategory() const;
	/// Gets the manufacture's requirements.
	const std::vector<const RuleResearch*> &getRequirements() const;
	/// Gets the base requirements.
	const std::vector<std::string> &getRequireBaseFunc() const;
	/// Gets the required workshop space.
	int getRequiredSpace() const;
	/// Gets the time required to manufacture one object.
	int getManufactureTime() const;
	/// Gets the cost of manufacturing one object.
	int getManufactureCost() const;
	/// Should all resources of a cancelled project be refunded?
	bool getRefund() const;
	/// Gets the list of items required to manufacture one object.
	const std::map<const RuleItem*, int> &getRequiredItems() const;
	/// Gets the list of crafts required to manufacture one object.
	const std::map<const RuleCraft*, int> &getRequiredCrafts() const;
	/// Gets the list of items produced by completing "one object" of this project.
	/// by default: it contains only the "name" item with a value of 1.
	const std::map<const RuleItem*, int> &getProducedItems() const;
	/// If this produce craft return its type, otherweasie null.
	const RuleCraft* getProducedCraft() const;
	/// Gets the "manufactured person", i.e. person spawned when manufacturing project ends.
	const std::string &getSpawnedPersonType() const;
	/// Gets the custom name of the "manufactured person".
	const std::string &getSpawnedPersonName() const;
	/// Gets the list weight for this manufacture item.
	int getListOrder() const;
};

}
