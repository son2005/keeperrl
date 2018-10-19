#include "stdafx.h"
#include "util.h"
#include "creature.h"
#include "collective_config.h"
#include "tribe.h"
#include "game.h"
#include "technology.h"
#include "collective_warning.h"
#include "item_type.h"
#include "resource_id.h"
#include "inventory.h"
#include "workshops.h"
#include "lasting_effect.h"
#include "item.h"
#include "view_id.h"
#include "furniture_type.h"
#include "minion_activity.h"
#include "furniture_usage.h"
#include "creature_attributes.h"
#include "collective.h"
#include "zones.h"
#include "construction_map.h"
#include "item_class.h"
#include "villain_type.h"
#include "furniture.h"
#include "immigrant_info.h"
#include "tutorial_highlight.h"
#include "trap_type.h"
#include "spell_id.h"
#include "spell.h"
#include "creature_factory.h"
#include "resource_info.h"
#include "workshop_item.h"
#include "body.h"
#include "view_id.h"
#include "view_object.h"
#include "territory.h"
#include "furniture_factory.h"
#include "storage_id.h"

template <class Archive>
void CollectiveConfig::serialize(Archive& ar, const unsigned int version) {
  ar(immigrantInterval, maxPopulation, immigrantInfo);
  ar(type, leaderAsFighter, spawnGhosts, ghostProb, guardianInfo, regenerateMana);
}

SERIALIZABLE(CollectiveConfig);
SERIALIZATION_CONSTRUCTOR_IMPL(CollectiveConfig);

template <class Archive>
void AttractionInfo::serialize(Archive& ar, const unsigned int version) {
  ar(types, amountClaimed);
}

SERIALIZABLE(AttractionInfo);
SERIALIZATION_CONSTRUCTOR_IMPL(AttractionInfo);

template <class Archive>
void GuardianInfo::serialize(Archive& ar, const unsigned int version) {
  ar(creature, probability, minEnemies, minVictims);
}

SERIALIZABLE(GuardianInfo);

static bool isSleepingFurniture(FurnitureType t) {
  switch (t) {
    case FurnitureType::BED1:
    case FurnitureType::BED2:
    case FurnitureType::BED3:
    case FurnitureType::BEAST_CAGE:
    case FurnitureType::COFFIN1:
    case FurnitureType::COFFIN2:
    case FurnitureType::COFFIN3:
    case FurnitureType::PRISON:
      return true;
    default:
      return false;
  }
}

static optional<FurnitureType> getBedType(WConstCreature c) {
  if (!c->getBody().needsToSleep())
    return none;
  if (c->getStatus().contains(CreatureStatus::PRISONER))
    return FurnitureType::PRISON;
  if (c->getBody().isUndead())
    return FurnitureType::COFFIN1;
  if (c->getBody().isHumanoid())
    return FurnitureType::BED1;
  else
    return FurnitureType::BEAST_CAGE;
}

void CollectiveConfig::addBedRequirementToImmigrants() {
  for (auto& info : immigrantInfo) {
    PCreature c = CreatureFactory::fromId(info.getId(0), TribeId::getDarkKeeper());
    if (info.getInitialRecruitment() == 0)
      if (auto bedType = getBedType(c.get())) {
        bool hasBed = false;
        info.visitRequirements(makeVisitor(
            [&](const AttractionInfo& attraction) -> void {
              for (auto& type : attraction.types)
                if (type == *bedType)
                  hasBed = true;
            },
            [&](const auto&) {}
        ));
        if (!hasBed)
          info.addRequirement(AttractionInfo(1, *bedType));
      }
  }
}

CollectiveConfig::CollectiveConfig(TimeInterval interval, const vector<ImmigrantInfo>& im, CollectiveType t, int maxPop)
    : immigrantInterval(interval), maxPopulation(maxPop), immigrantInfo(im), type(t) {
  if (type == KEEPER)
    addBedRequirementToImmigrants();
}

CollectiveConfig CollectiveConfig::keeper(TimeInterval immigrantInterval, int maxPopulation, bool regenerateMana,
    const vector<ImmigrantInfo>& im) {
  auto ret = CollectiveConfig(immigrantInterval, im, KEEPER, maxPopulation);
  ret.regenerateMana = regenerateMana;
  return ret;
}

CollectiveConfig CollectiveConfig::withImmigrants(TimeInterval interval, int maxPopulation, const vector<ImmigrantInfo>& im) {
  return CollectiveConfig(interval, im, VILLAGE, maxPopulation);
}

CollectiveConfig CollectiveConfig::noImmigrants() {
  return CollectiveConfig(TimeInterval {}, {}, VILLAGE, 10000);
}

CollectiveConfig& CollectiveConfig::setLeaderAsFighter() {
  leaderAsFighter = true;
  return *this;
}

CollectiveConfig& CollectiveConfig::setGhostSpawns(double prob, int num) {
  ghostProb = prob;
  spawnGhosts = num;
  return *this;
}

int CollectiveConfig::getNumGhostSpawns() const {
  return spawnGhosts;
}

TimeInterval CollectiveConfig::getImmigrantTimeout() const {
  return 500_visible;
}

double CollectiveConfig::getGhostProb() const {
  return ghostProb;
}

bool CollectiveConfig::isLeaderFighter() const {
  return leaderAsFighter;
}

bool CollectiveConfig::getManageEquipment() const {
  return type == KEEPER;
}

bool CollectiveConfig::getFollowLeaderIfNoTerritory() const {
  return type == KEEPER;
}

bool CollectiveConfig::stayInTerritory() const {
  return type != KEEPER;
}

bool CollectiveConfig::hasVillainSleepingTask() const {
  return type != KEEPER;
}

bool CollectiveConfig::getRegenerateMana() const {
  return regenerateMana;
}

bool CollectiveConfig::allowHealingTaskOutsideTerritory() const {
  return type == KEEPER;
}

bool CollectiveConfig::hasImmigrantion(bool currentlyActiveModel) const {
  return type != KEEPER || currentlyActiveModel;
}

TimeInterval CollectiveConfig::getImmigrantInterval() const {
  return immigrantInterval;
}

bool CollectiveConfig::getStripSpawns() const {
  return type == KEEPER;
}

bool CollectiveConfig::getEnemyPositions() const {
  return type == KEEPER;
}

bool CollectiveConfig::getWarnings() const {
  return type == KEEPER;
}

bool CollectiveConfig::getConstructions() const {
  return type == KEEPER;
}

bool CollectiveConfig::bedsLimitImmigration() const {
  return type == KEEPER;
}

int CollectiveConfig::getMaxPopulation() const {
  return maxPopulation;
}

const vector<ImmigrantInfo>& CollectiveConfig::getImmigrantInfo() const {
  return immigrantInfo;
}

CollectiveConfig& CollectiveConfig::setGuardian(GuardianInfo info) {
  guardianInfo = info;
  return *this;
}

const optional<GuardianInfo>& CollectiveConfig::getGuardianInfo() const {
  return guardianInfo;
}

const vector<FurnitureType>& CollectiveConfig::getRoomsNeedingLight() const {
  static vector<FurnitureType> ret {
    FurnitureType::WORKSHOP,
    FurnitureType::FORGE,
    FurnitureType::LABORATORY,
    FurnitureType::JEWELER,
    FurnitureType::TRAINING_WOOD,
    FurnitureType::TRAINING_IRON,
    FurnitureType::TRAINING_ADA,
    FurnitureType::BOOKCASE_WOOD,
    FurnitureType::BOOKCASE_IRON,
    FurnitureType::BOOKCASE_GOLD,
  };
  return ret;
};

bool CollectiveConfig::canBuildOutsideTerritory(FurnitureType type) {
  switch (type) {
    case FurnitureType::EYEBALL:
    case FurnitureType::KEEPER_BOARD:
    case FurnitureType::DUNGEON_WALL:
    case FurnitureType::DUNGEON_WALL2:
    case FurnitureType::TORCH_N:
    case FurnitureType::TORCH_E:
    case FurnitureType::TORCH_S:
    case FurnitureType::TORCH_W:
    case FurnitureType::WOOD_WALL:
    case FurnitureType::CASTLE_WALL:
    case FurnitureType::TUTORIAL_ENTRANCE:
    case FurnitureType::PIT:
    case FurnitureType::GRAVE:
    case FurnitureType::BRIDGE: return true;
    default: return false;
  }
}

const ResourceInfo& CollectiveConfig::getResourceInfo(CollectiveResourceId id) {
  static EnumMap<CollectiveResourceId, ResourceInfo> resourceInfo([](CollectiveResourceId id)->ResourceInfo {
    switch (id) {
      case CollectiveResourceId::PRISONER_HEAD:
        return { none, none, ItemType::GoldPiece{}, "", ViewId::IMPALED_HEAD, true};
      case CollectiveResourceId::GOLD:
        return {StorageId::GOLD, ItemIndex::GOLD, ItemType::GoldPiece{}, "gold", ViewId::GOLD};
      case CollectiveResourceId::WOOD:
        return { StorageId::RESOURCE, ItemIndex::WOOD, ItemType::WoodPlank{}, "wood", ViewId::WOOD_PLANK,
            false, TutorialHighlight::WOOD_RESOURCE};
      case CollectiveResourceId::IRON:
        return { StorageId::RESOURCE, ItemIndex::IRON, ItemType::IronOre{}, "iron", ViewId::IRON_ROCK};
      case CollectiveResourceId::ADA:
        return { StorageId::RESOURCE, ItemIndex::ADA, ItemType::AdaOre{}, "adamantium", ViewId::ADA_ORE};
      case CollectiveResourceId::STONE:
        return { StorageId::RESOURCE, ItemIndex::STONE, ItemType::Rock{}, "granite", ViewId::ROCK};
      case CollectiveResourceId::CORPSE:
        return { StorageId::CORPSES, ItemIndex::REVIVABLE_CORPSE, ItemType::GoldPiece{}, "corpses", ViewId::BODY_PART, true};
    }
  });
  return resourceInfo[id];
}

static CollectiveItemPredicate unMarkedItems() {
  return [](WConstCollective col, WConstItem it) { return !col->isItemMarked(it); };
}


const vector<ItemFetchInfo>& CollectiveConfig::getFetchInfo() const {
  if (type == KEEPER) {
    static vector<ItemFetchInfo> ret {
        {ItemIndex::CORPSE, unMarkedItems(), StorageId::CORPSES, CollectiveWarning::GRAVES},
        {ItemIndex::GOLD, unMarkedItems(), StorageId::GOLD, CollectiveWarning::CHESTS},
        {ItemIndex::MINION_EQUIPMENT, [](WConstCollective col, WConstItem it)
            { return it->getClass() != ItemClass::GOLD && !col->isItemMarked(it);},
            StorageId::EQUIPMENT, CollectiveWarning::EQUIPMENT_STORAGE},
        {ItemIndex::WOOD, unMarkedItems(), StorageId::RESOURCE,
            CollectiveWarning::RESOURCE_STORAGE},
        {ItemIndex::IRON, unMarkedItems(), StorageId::RESOURCE,
            CollectiveWarning::RESOURCE_STORAGE},
        {ItemIndex::ADA, unMarkedItems(), StorageId::RESOURCE,
            CollectiveWarning::RESOURCE_STORAGE},
        {ItemIndex::STONE, unMarkedItems(), StorageId::RESOURCE,
            CollectiveWarning::RESOURCE_STORAGE},
        /*{ItemIndex::TRAP, unMarkedItems(), [](WConstCollective col) -> const PositionSet& {
                return col->getTerritory().getAllAsSet(); },
            CollectiveWarning::RESOURCE_STORAGE},*/
    };
    return ret;
  } else {
    static vector<ItemFetchInfo> empty;
    return empty;
  }
}

MinionActivityInfo::MinionActivityInfo(Type t, const string& desc) : type(t), description(desc) {
  CHECK(type != FURNITURE);
}

MinionActivityInfo::MinionActivityInfo() {}

MinionActivityInfo::MinionActivityInfo(FurnitureType type, const string& desc, bool requiresLighting)
    : type(FURNITURE),
      furniturePredicate([type](WConstCollective, WConstCreature, FurnitureType t) { return t == type;}),
      description(desc),
      requiresLighting(requiresLighting) {
}

MinionActivityInfo::MinionActivityInfo(UsagePredicate pred, const string& desc, bool requiresLighting)
    : type(FURNITURE), furniturePredicate(pred), description(desc), requiresLighting(requiresLighting) {
}

static EnumMap<WorkshopType, WorkshopInfo> workshops([](WorkshopType type)->WorkshopInfo {
  switch (type) {
    case WorkshopType::WORKSHOP: return {FurnitureType::WORKSHOP, "workshop", SkillId::WORKSHOP};
    case WorkshopType::FORGE: return {FurnitureType::FORGE, "forge", SkillId::FORGE};
    case WorkshopType::LABORATORY: return {FurnitureType::LABORATORY, "laboratory", SkillId::LABORATORY};
    case WorkshopType::JEWELER: return {FurnitureType::JEWELER, "jeweler", SkillId::JEWELER};
  }});

optional<WorkshopType> CollectiveConfig::getWorkshopType(FurnitureType furniture) {
  static optional<EnumMap<FurnitureType, optional<WorkshopType>>> map;
  if (!map) {
    map.emplace();
    for (auto type : ENUM_ALL(WorkshopType))
      (*map)[workshops[type].furniture] = type;
  }
  return (*map)[furniture];
}

map<CollectiveResourceId, int> CollectiveConfig::getStartingResource() const {
  return map<CollectiveResourceId, int>{};
}

const vector<FurnitureType>& CollectiveConfig::getTrainingFurniture(ExperienceType type) {
  static EnumMap<ExperienceType, vector<FurnitureType>> ret(
      [](ExperienceType expType) {
        vector<FurnitureType> furniture;
        for (auto type : ENUM_ALL(FurnitureType))
          if (!!getTrainingMaxLevel(expType, type))
            furniture.push_back(type);
        return furniture;
      });
  return ret[type];
}

optional<int> CollectiveConfig::getTrainingMaxLevel(ExperienceType experienceType, FurnitureType type) {
  switch (experienceType) {
    case ExperienceType::MELEE:
      switch (type) {
        case FurnitureType::TRAINING_WOOD:
          return 3;
        case FurnitureType::TRAINING_IRON:
          return 7;
        case FurnitureType::TRAINING_ADA:
          return 12;
        default:
          return none;
      }
      break;
    case ExperienceType::SPELL:
      switch (type) {
        case FurnitureType::BOOKCASE_WOOD:
          return 3;
        case FurnitureType::BOOKCASE_IRON:
          return 7;
        case FurnitureType::BOOKCASE_GOLD:
          return 12;
        default:
          return none;
      }
      break;
    default:
      return none;
  }
}

int CollectiveConfig::getManaForConquering(const optional<VillainType>& type) {
  if (type)
    switch (*type) {
      case VillainType::MAIN:
        return 200;
      case VillainType::LESSER:
        return 100;
      default:
        break;
    }
  return 50;
}

CollectiveConfig::CollectiveConfig(const CollectiveConfig&) = default;

CollectiveConfig::~CollectiveConfig() {
}

static auto getTrainingPredicate(ExperienceType experienceType) {
  return [experienceType] (WConstCollective, WConstCreature c, FurnitureType t) {
      if (auto maxIncrease = CollectiveConfig::getTrainingMaxLevel(experienceType, t))
        return !c || (c->getAttributes().getExpLevel(experienceType) < *maxIncrease &&
            !c->getAttributes().isTrainingMaxedOut(experienceType));
      else
        return false;
    };
}

template <typename Pred>
static auto addManaGenerationPredicate(Pred p) {
  return [p] (WConstCollective col, WConstCreature c, FurnitureType t) {
    return (!!CollectiveConfig::getTrainingMaxLevel(ExperienceType::SPELL, t) &&
            (!col || col->getConfig().getRegenerateMana()))
        || p(col, c, t);
  };
}

bool CollectiveConfig::requiresLighting(FurnitureType type) {
  static EnumMap<FurnitureType, bool> result([] (FurnitureType type) {
    for (auto activity : ENUM_ALL(MinionActivity)) {
      auto& info = getActivityInfo(activity);
      if (info.type == MinionActivityInfo::FURNITURE && info.requiresLighting &&
          info.furniturePredicate(nullptr, nullptr, type))
        return true;
    }
    return false;
  });
  return result[type];
}

const MinionActivityInfo& CollectiveConfig::getActivityInfo(MinionActivity task) {
  static EnumMap<MinionActivity, MinionActivityInfo> map([](MinionActivity task) -> MinionActivityInfo {
    switch (task) {
      case MinionActivity::IDLE: return {MinionActivityInfo::IDLE, "idle"};
      case MinionActivity::CONSTRUCTION: return {MinionActivityInfo::WORKER, "construction"};
      case MinionActivity::HAULING: return {MinionActivityInfo::WORKER, "hauling"};
      case MinionActivity::WORKING: return {MinionActivityInfo::WORKER, "labour"};
      case MinionActivity::DIGGING: return {MinionActivityInfo::WORKER, "digging"};
      case MinionActivity::TRAIN: return {getTrainingPredicate(ExperienceType::MELEE), "training", true};
      case MinionActivity::SLEEP: return {[](WConstCollective, WConstCreature c, FurnitureType t) {
            if (!c)
              return isSleepingFurniture(t);
            auto bedType = getBedType(c);
            return t == bedType || (bedType && FurnitureFactory::isUpgrade(*bedType, t));
          }, "sleeping", false};
      case MinionActivity::EAT: return {MinionActivityInfo::EAT, "eating"};
      case MinionActivity::THRONE: return {FurnitureType::THRONE, "throne", true};
      case MinionActivity::STUDY: return {addManaGenerationPredicate(getTrainingPredicate(ExperienceType::SPELL)),
           "studying", true};
      case MinionActivity::CROPS: return {FurnitureType::CROPS, "crops", false};
      case MinionActivity::RITUAL: return {FurnitureType::DEMON_SHRINE, "rituals", false};
      case MinionActivity::ARCHERY: return {MinionActivityInfo::ARCHERY, "archery range"};
      case MinionActivity::COPULATE: return {MinionActivityInfo::COPULATE, "copulation"};
      case MinionActivity::EXPLORE: return {MinionActivityInfo::EXPLORE, "spying"};
      case MinionActivity::SPIDER: return {MinionActivityInfo::SPIDER, "spider"};
      case MinionActivity::EXPLORE_NOCTURNAL: return {MinionActivityInfo::EXPLORE, "spying"};
      case MinionActivity::EXPLORE_CAVES: return {MinionActivityInfo::EXPLORE, "spying"};
      case MinionActivity::BE_WHIPPED: return {FurnitureType::WHIPPING_POST, "being whipped", false};
      case MinionActivity::BE_TORTURED: return {FurnitureType::TORTURE_TABLE, "being tortured", false};
      case MinionActivity::BE_EXECUTED: return {FurnitureType::GALLOWS, "being executed", false};
      case MinionActivity::CRAFT: return {[](WConstCollective col, WConstCreature c, FurnitureType t) {
            if (auto type = getWorkshopType(t))
              return !c || !col || (c->getAttributes().getSkills().getValue(getWorkshopInfo(*type).skill) > 0 &&
                            !col->getWorkshops().get(*type).isIdle());
            else
              return false;
          },
          "crafting", true};
    }
  });
  return map[task];
}

const WorkshopInfo& CollectiveConfig::getWorkshopInfo(WorkshopType type) {
  return workshops[type];
}


unique_ptr<Workshops> CollectiveConfig::getWorkshops() const {
  return unique_ptr<Workshops>(new Workshops({
      {WorkshopType::WORKSHOP, {
          Workshops::Item::fromType(ItemType::LeatherArmor{}, 6, {CollectiveResourceId::WOOD, 20}),
          Workshops::Item::fromType(ItemType::LeatherHelm{}, 1, {CollectiveResourceId::WOOD, 6}),
          Workshops::Item::fromType(ItemType::LeatherBoots{}, 2, {CollectiveResourceId::WOOD, 10}),
          Workshops::Item::fromType(ItemType::LeatherGloves{}, 1, {CollectiveResourceId::WOOD, 2}),
          Workshops::Item::fromType(ItemType::Club{}, 3, {CollectiveResourceId::WOOD, 10})
              .setTutorialHighlight(TutorialHighlight::SCHEDULE_CLUB),
          Workshops::Item::fromType(ItemType::HeavyClub{}, 5, {CollectiveResourceId::WOOD, 20})
                  .setTechId(TechId::TWO_H_WEAP),
          Workshops::Item::fromType(ItemType::Bow{}, 13, {CollectiveResourceId::WOOD, 20}).setTechId(TechId::ARCHERY),
          Workshops::Item::fromType(ItemType::WoodenStaff{}, 13, {CollectiveResourceId::WOOD, 20})
                  .setTechId(TechId::MAGICAL_WEAPONS),
          Workshops::Item::fromType(ItemType::Torch{}, 2, {CollectiveResourceId::WOOD, 4}),
          Workshops::Item::fromType(ItemType::TrapItem{TrapType::BOULDER}, 20, {CollectiveResourceId::STONE, 120})
                  .setTechId(TechId::TRAPS),
          Workshops::Item::fromType(ItemType::TrapItem{TrapType::POISON_GAS}, 10, {CollectiveResourceId::WOOD, 20})
                  .setTechId(TechId::TRAPS),
          Workshops::Item::fromType(ItemType::TrapItem{TrapType::ALARM}, 8, {CollectiveResourceId::WOOD, 20})
                  .setTechId(TechId::TRAPS),
          Workshops::Item::fromType(ItemType::TrapItem{TrapType::WEB}, 8, {CollectiveResourceId::WOOD, 20})
                  .setTechId(TechId::TRAPS),
          Workshops::Item::fromType(ItemType::TrapItem{TrapType::SURPRISE}, 8, {CollectiveResourceId::WOOD, 20})
                  .setTechId(TechId::TRAPS),
          Workshops::Item::fromType(ItemType::TrapItem{TrapType::TERROR}, 8, {CollectiveResourceId::WOOD, 20})
                  .setTechId(TechId::TRAPS),
      }},
      {WorkshopType::FORGE, {
          Workshops::Item::fromType(ItemType::Sword{}, 10, {CollectiveResourceId::IRON, 20}),
          Workshops::Item::fromType(ItemType::ChainArmor{}, 30, {CollectiveResourceId::IRON, 40}),
          Workshops::Item::fromType(ItemType::IronHelm{}, 8, {CollectiveResourceId::IRON, 16}),
          Workshops::Item::fromType(ItemType::IronBoots{}, 12, {CollectiveResourceId::IRON, 24}),
          Workshops::Item::fromType(ItemType::WarHammer{}, 16, {CollectiveResourceId::IRON, 40})
                  .setTechId(TechId::TWO_H_WEAP),
          Workshops::Item::fromType(ItemType::BattleAxe{}, 22, {CollectiveResourceId::IRON, 50})
                  .setTechId(TechId::TWO_H_WEAP),
          Workshops::Item::fromType(ItemType::IronStaff{}, 20, {CollectiveResourceId::IRON, 40})
                 .setTechId(TechId::MAGICAL_WEAPONS),
          Workshops::Item::fromType(ItemType::AdaSword{}, 20, {CollectiveResourceId::ADA, 20}),
          Workshops::Item::fromType(ItemType::AdaArmor{}, 30, {CollectiveResourceId::ADA, 40}),
          Workshops::Item::fromType(ItemType::AdaBattleAxe{}, 22, {CollectiveResourceId::ADA, 50})
	         .setTechId(TechId::TWO_H_WEAP),
      }},
      {WorkshopType::LABORATORY, {
          Workshops::Item::fromType(ItemType::Potion{Effect::Lasting{LastingEffect::SLOWED}}, 2,
              {CollectiveResourceId::GOLD, 2}),
          Workshops::Item::fromType(ItemType::Potion{Effect::Lasting{LastingEffect::SLEEP}}, 2,
              {CollectiveResourceId::GOLD, 2}),
          Workshops::Item::fromType(ItemType::Potion{
              Effect::Lasting{LastingEffect::POISON_RESISTANT}}, 4, {CollectiveResourceId::GOLD, 6}),
          Workshops::Item::fromType(ItemType::Potion{
              Effect::Lasting{LastingEffect::SPEED}}, 4, {CollectiveResourceId::GOLD, 6}),
          Workshops::Item::fromType(ItemType::Potion{
              Effect::Lasting{LastingEffect::TELEPATHY}}, 4, {CollectiveResourceId::GOLD, 6}),
          Workshops::Item::fromType(ItemType::Potion{
              Effect::Lasting{LastingEffect::REGENERATION}}, 4, {CollectiveResourceId::GOLD, 8}),
          Workshops::Item::fromType(ItemType::Potion{
              Effect::Lasting{LastingEffect::POISON}}, 4, {CollectiveResourceId::GOLD, 8}),
          Workshops::Item::fromType(ItemType::Potion{
              Effect::Lasting{LastingEffect::FLYING}}, 4, {CollectiveResourceId::GOLD, 8}),
          Workshops::Item::fromType(ItemType::Potion{Effect::Heal{}}, 4, {CollectiveResourceId::GOLD, 10})
             .setTechId(TechId::ALCHEMY_ADV),
          Workshops::Item::fromType(ItemType::Potion{
              Effect::Lasting{LastingEffect::BLIND}}, 4, {CollectiveResourceId::GOLD, 15})
                  .setTechId(TechId::ALCHEMY_ADV),
          Workshops::Item::fromType(ItemType::Potion{
              Effect::Lasting{LastingEffect::MELEE_RESISTANCE}}, 6, {CollectiveResourceId::GOLD, 20})
                  .setTechId(TechId::ALCHEMY_ADV),
          Workshops::Item::fromType(ItemType::Potion{
              Effect::Lasting{LastingEffect::MELEE_VULNERABILITY}}, 6, {CollectiveResourceId::GOLD, 40})
                  .setTechId(TechId::ALCHEMY_ADV),
          Workshops::Item::fromType(ItemType::Potion{
              Effect::Lasting{LastingEffect::MAGIC_VULNERABILITY}}, 6, {CollectiveResourceId::GOLD, 40})
                  .setTechId(TechId::ALCHEMY_ADV),
          Workshops::Item::fromType(ItemType::Potion{
              Effect::Lasting{LastingEffect::INVISIBLE}}, 6, {CollectiveResourceId::GOLD, 20})
                  .setTechId(TechId::ALCHEMY_ADV),
          //Alchemical conversion to and from gold
          Workshops::Item::fromType(ItemType::GoldPiece{}, 5, {CollectiveResourceId::IRON, 30})
              .setTechId(TechId::ALCHEMY_CONV).setBatchSize(10),
          Workshops::Item::fromType(ItemType::WoodPlank{}, 5, {CollectiveResourceId::GOLD, 10})
              .setTechId(TechId::ALCHEMY_CONV).setBatchSize(10),
          Workshops::Item::fromType(ItemType::IronOre{}, 5, {CollectiveResourceId::GOLD, 10})
              .setTechId(TechId::ALCHEMY_CONV).setBatchSize(10),
          Workshops::Item::fromType(ItemType::AdaOre{}, 5, {CollectiveResourceId::GOLD, 40})
              .setTechId(TechId::ALCHEMY_CONV).setBatchSize(10),
         Workshops::Item::fromType(ItemType::Rock{}, 5, {CollectiveResourceId::GOLD, 10})
             .setTechId(TechId::ALCHEMY_CONV).setBatchSize(10),
      }},
      {WorkshopType::JEWELER, {
          Workshops::Item::fromType(ItemType::Ring{LastingEffect::POISON_RESISTANT}, 10,
              {CollectiveResourceId::GOLD, 20}),
          Workshops::Item::fromType(ItemType::Ring{LastingEffect::FIRE_RESISTANT}, 10,
              {CollectiveResourceId::GOLD, 30}),
          Workshops::Item::fromType(ItemType::Ring{LastingEffect::MAGIC_RESISTANCE}, 10,
              {CollectiveResourceId::GOLD, 30}),
          Workshops::Item::fromType(ItemType::Ring{LastingEffect::RESTED}, 10, {CollectiveResourceId::GOLD, 30}),
          Workshops::Item::fromType(ItemType::Ring{LastingEffect::SATIATED}, 10, {CollectiveResourceId::GOLD, 30}),
          Workshops::Item::fromType(ItemType::Amulet{LastingEffect::NIGHT_VISION}, 10, {CollectiveResourceId::GOLD, 20}),
          Workshops::Item::fromType(ItemType::Amulet{LastingEffect::ELF_VISION}, 10, {CollectiveResourceId::GOLD, 20}),
          Workshops::Item::fromType(ItemType::Amulet{LastingEffect::WARNING}, 10, {CollectiveResourceId::GOLD, 30}),
          Workshops::Item::fromType(ItemType::DefenseAmulet{}, 10, {CollectiveResourceId::GOLD, 40}),
          Workshops::Item::fromType(ItemType::Amulet{LastingEffect::REGENERATION}, 10, {CollectiveResourceId::GOLD, 60}),
      }},
  }));
}
