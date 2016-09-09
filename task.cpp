/* Copyright (C) 2013-2014 Michal Brzozowski (rusolis@poczta.fm)

   This file is part of KeeperRL.

   KeeperRL is free software; you can redistribute it and/or modify it under the terms of the
   GNU General Public License as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   KeeperRL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without
   even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along with this program.
   If not, see http://www.gnu.org/licenses/ . */

#include "stdafx.h"

#include "task.h"
#include "level.h"
#include "entity_set.h"
#include "item.h"
#include "creature.h"
#include "collective.h"
#include "trigger.h"
#include "location.h"
#include "square_factory.h"
#include "equipment.h"
#include "tribe.h"
#include "skill.h"
#include "creature_name.h"
#include "collective_teams.h"
#include "effect.h"
#include "creature_factory.h"
#include "player_message.h"
#include "territory.h"
#include "item_factory.h"
#include "game.h"
#include "model.h"
#include "collective_name.h"
#include "creature_attributes.h"
#include "square_type.h"
#include "item_type.h"
#include "body.h"
#include "furniture_type.h"
#include "construction_map.h"
#include "furniture.h"

template <class Archive> 
void Task::serialize(Archive& ar, const unsigned int version) {
  ar& SUBCLASS(UniqueEntity)
    & SVAR(done)
    & SVAR(transfer);
}

SERIALIZABLE(Task);

Task::Task(bool t) : transfer(t) {
}

Task::~Task() {
}

bool Task::isImpossible(const Level*) {
  return false;
}

bool Task::canTransfer() {
  return transfer;
}

bool Task::canPerform(const Creature* c) {
  return true;
}

bool Task::isDone() {
  return done;
}

void Task::setDone() {
  done = true;
}

namespace {

class ConstructionHelper {
  public:
  ConstructionHelper(SquareType s) : square(s) {}
  ConstructionHelper(FurnitureType f) : furniture(f) {}

  bool canConstruct(Position pos) {
    if (square)
      return pos.canConstruct(*square);
    else
      return pos.canConstruct(*furniture);
  }

  string getDescription(Position position) const {
    if (square)
      switch (square->getId()) {
        case SquareId::FLOOR: return "Dig " + toString(position);
        default: return "Build " + transform2(EnumInfo<SquareId>::getString(square->getId()),
                   [] (char c) -> char { if (c == '_') return ' '; else return tolower(c); }) + toString(position);
      }
    else
      return "Build " + transform2(EnumInfo<FurnitureType>::getString(*furniture),
          [] (char c) -> char { if (c == '_') return ' '; else return tolower(c); }) + toString(position);

  }

  CreatureAction getAction(Creature* c, Vec2 dir) {
    if (square)
      return c->construct(dir, *square);
    else
      return c->construct(dir, *furniture);
  }

  void onConstructed(TaskCallback* callback, Position position) {
    if (square)
      callback->onConstructed(position, *square);
    else
      callback->onConstructed(position, *furniture);
  }

  SERIALIZE_ALL(square, furniture);
  SERIALIZATION_CONSTRUCTOR(ConstructionHelper);

  private:
  optional<SquareType> SERIAL(square);
  optional<FurnitureType> SERIAL(furniture);
};

class Construction : public Task {
  public:
  Construction(TaskCallback* c, Position pos, const ConstructionHelper& h) : Task(true), helper(h), position(pos),
      callback(c) {}

  virtual bool isImpossible(const Level* level) override {
    return !helper.canConstruct(position);
  }

  virtual string getDescription() const override {
    return helper.getDescription(position);
  }

  virtual MoveInfo getMove(Creature* c) override {
    if (!callback->isConstructionReachable(position))
      return NoMove;
    if (c->getPosition().dist8(position) > 1)
      return c->moveTowards(position);
    CHECK(c->getAttributes().getSkills().hasDiscrete(SkillId::CONSTRUCTION));
    Vec2 dir = c->getPosition().getDir(position);
    if (auto action = helper.getAction(c, dir))
      return {1.0, action.append([=](Creature* c) {
          if (!position.isActiveConstruction()) {
            setDone();
            helper.onConstructed(callback, position);
          }
          })};
    else {
      setDone();
      return NoMove;
    }
  }

  SERIALIZE_ALL2(Task, position, helper, callback);
  SERIALIZATION_CONSTRUCTOR(Construction);

  private:
  ConstructionHelper SERIAL(helper);
  Position SERIAL(position);
  TaskCallback* SERIAL(callback);
};

}

PTask Task::construction(TaskCallback* c, Position target, const SquareType& type) {
  return PTask(new Construction(c, target, type));
}

PTask Task::construction(TaskCallback* c, Position target, FurnitureType type) {
  return PTask(new Construction(c, target, type));
}

namespace {
class Destruction : public Task {
  public:
  Destruction(TaskCallback* c, Position pos, Furniture* furniture, DestroyAction::Value action) : Task(true), position(pos),
      callback(c), destroyAction(action), description(DestroyAction::getVerbSecondPerson(destroyAction) +
          " "_s + furniture->getName()), furnitureId(furniture->getUniqueId()) {}

  virtual bool isImpossible(const Level* level) override {
    return !position.isDestroyable();
  }

  virtual string getDescription() const override {
    return description;
  }

  virtual MoveInfo getMove(Creature* c) override {
    if (!callback->isConstructionReachable(position))
      return NoMove;
    if (c->getPosition().dist8(position) > 1)
      return c->moveTowards(position);
    CHECK(c->getAttributes().getSkills().hasDiscrete(SkillId::CONSTRUCTION));
    Vec2 dir = c->getPosition().getDir(position);
    if (auto action = c->destroy(dir, destroyAction))
      return {1.0, action.append([=](Creature* c) {
          if (!position.getFurniture() || position.getFurniture()->getUniqueId() != furnitureId) {
            setDone();
            callback->onDestructedFurniture(position);
          }
          })};
    else {
      setDone();
      return NoMove;
    }
  }

  SERIALIZE_ALL2(Task, position, callback, destroyAction, description, furnitureId)
  SERIALIZATION_CONSTRUCTOR(Destruction)

  private:
  Position SERIAL(position);
  TaskCallback* SERIAL(callback);
  DestroyAction::Value SERIAL(destroyAction);
  string SERIAL(description);
  Furniture::Id SERIAL(furnitureId);
};

}

PTask Task::destruction(TaskCallback* c, Position target, Furniture* furniture, DestroyAction::Value destroyAction) {
  return PTask(new Destruction(c, target, furniture, destroyAction));
}

namespace {

class BuildTorch : public Task {
  public:
  BuildTorch(TaskCallback* c, Position pos, Dir dir) : Task(true), position(pos), callback(c), attachmentDir(dir) {}

  virtual MoveInfo getMove(Creature* c) override {
    CHECK(c->getAttributes().getSkills().hasDiscrete(SkillId::CONSTRUCTION));
    if (c->getPosition() == position)
      return c->placeTorch(attachmentDir, [=](Trigger* t) {
          callback->onTorchBuilt(position, t);
          setDone();
        });
    else
      return c->moveTowards(position);
  }

  virtual string getDescription() const override {
    return "Build torch " + toString(position);
  }

  SERIALIZE_ALL2(Task, position, callback, attachmentDir); 
  SERIALIZATION_CONSTRUCTOR(BuildTorch);

  private:
  Position SERIAL(position);
  TaskCallback* SERIAL(callback);
  Dir SERIAL(attachmentDir);
};

}

PTask Task::buildTorch(TaskCallback* call, Position target, Dir attachmentDir) {
  return PTask(new BuildTorch(call, target, attachmentDir));
}

namespace {

class PickItem : public Task {
  public:
  PickItem(TaskCallback* c, Position pos, vector<Item*> _items, int retries = 10)
      : items(_items), position(pos), callback(c), tries(retries) {
    CHECK(!items.empty());
  }

  virtual void onPickedUp() {
    setDone();
  }

  Position getPosition() {
    return position;
  }

  bool itemsExist(Position target) {
    for (const Item* it : target.getItems())
      if (items.contains(it))
        return true;
    return false;
  }

  virtual string getDescription() const override {
    return "Pick item " + toString(position);
  }

  virtual void cancel() override {
    callback->onCantPickItem(items);
  }

  virtual MoveInfo getMove(Creature* c) override {
    CHECK(!pickedUp);
    if (!itemsExist(position)) {
      callback->onCantPickItem(items);
      setDone();
      return NoMove;
    }
    if (c->getPosition() == position) {
      vector<Item*> hereItems;
      for (Item* it : c->getPickUpOptions())
        if (items.contains(it)) {
          hereItems.push_back(it);
          items.erase(it);
        }
      callback->onCantPickItem(items);
      if (hereItems.empty()) {
        setDone();
        return NoMove;
      }
      items = hereItems;
      if (auto action = c->pickUp(hereItems))
        return {1.0, action.append([=](Creature* c) {
          pickedUp = true;
          onPickedUp();
          callback->onTaskPickedUp(position, hereItems);
        })}; 
      else {
        callback->onCantPickItem(items);
        setDone();
        return NoMove;
      }
    }
    if (auto action = c->moveTowards(position, true))
      return action;
    else if (--tries == 0) {
      callback->onCantPickItem(items);
      setDone();
    }
    return NoMove;
  }

  SERIALIZE_ALL2(Task, items, pickedUp, position, tries, callback); 
  SERIALIZATION_CONSTRUCTOR(PickItem);

  protected:
  EntitySet<Item> SERIAL(items);
  bool SERIAL(pickedUp) = false;
  Position SERIAL(position);
  TaskCallback* SERIAL(callback);
  int SERIAL(tries);
};
}

PTask Task::pickItem(TaskCallback* c, Position position, vector<Item*> items) {
  return PTask(new PickItem(c, position, items));
}

namespace {

class PickAndEquipItem : public PickItem {
  public:
  PickAndEquipItem(TaskCallback* c, Position position, vector<Item*> _items) : PickItem(c, position, _items) {
  }

  virtual void onPickedUp() override {
  }

  virtual string getDescription() const override {
    return "Pick and equip item " + toString(position);
  }

  virtual MoveInfo getMove(Creature* c) override {
    if (!pickedUp)
      return PickItem::getMove(c);
    vector<Item*> it = c->getEquipment().getItems(items.containsPredicate());
    if (!it.empty()) {
      if (auto action = c->equip(getOnlyElement(it)))
        return {1.0, action.append([=](Creature* c) {
          setDone();
        })};
    } else
      setDone();
    return NoMove;
  }

  SERIALIZE_SUBCLASS(PickItem);   
  SERIALIZATION_CONSTRUCTOR(PickAndEquipItem);
};

}

PTask Task::pickAndEquipItem(TaskCallback* c, Position position, Item* items) {
  return PTask(new PickAndEquipItem(c, position, {items}));
}

namespace {

class EquipItem : public Task {
  public:
  EquipItem(Item* item) : itemId(item->getUniqueId()) {
  }

  virtual string getDescription() const override {
    return "Equip item";
  }

  virtual MoveInfo getMove(Creature* c) override {
    CHECK(c->getBody().isHumanoid()) << c->getName().bare();
    if (Item* item = c->getEquipment().getItemById(itemId)) {
      if (auto action = c->equip(item))
        return action.append([=](Creature* c) {setDone();});
      setDone();
      return NoMove;
    } else { // either item was dropped or doesn't exist anymore.
      setDone();
      return NoMove;
    }
  }

  SERIALIZE_ALL2(Task, itemId); 
  SERIALIZATION_CONSTRUCTOR(EquipItem);

  private:
  UniqueEntity<Item>::Id SERIAL(itemId);
};
}

PTask Task::equipItem(Item* item) {
  return PTask(new EquipItem(item));
}

static Position chooseRandomClose(Position start, const vector<Position>& squares, Task::SearchType type) {
  CHECK(!squares.empty());
  int minD = 10000;
  int margin = type == Task::LAZY ? 0 : 3;
  vector<Position> close;
  for (Position v : squares)
    minD = min(minD, v.dist8(start));
  for (Position v : squares)
    if (v.dist8(start) <= minD + margin)
      close.push_back(v);
  if (close.empty())
    return Random.choose(squares);
  else
    return Random.choose(close);
}

class BringItem : public PickItem {
  public:
  BringItem(TaskCallback* c, Position position, vector<Item*> items, vector<Position> _target, int retries)
      : PickItem(c, position, items, retries), target(chooseRandomClose(position, _target, LAZY)) {}

  BringItem(TaskCallback* c, Position position, vector<Item*> items, vector<Position> _target)
      : PickItem(c, position, items), target(chooseRandomClose(position, _target, LAZY)) {}

  virtual CreatureAction getBroughtAction(Creature* c, vector<Item*> it) {
    return c->drop(it).append([=](Creature* c) {
        callback->onBrought(c->getPosition(), it);
    });
  }

  virtual string getDescription() const override {
    return "Bring item from " + toString(position) + " to " + toString(target);
  }

  virtual void onPickedUp() override {
  }
  
  virtual MoveInfo getMove(Creature* c) override {
    if (!pickedUp)
      return PickItem::getMove(c);
    if (c->getPosition() == target) {
      vector<Item*> myItems = c->getEquipment().getItems(items.containsPredicate());
      if (auto action = getBroughtAction(c, myItems))
        return {1.0, action.append([=](Creature* c) {setDone();})};
      else {
        setDone();
        return NoMove;
      }
    } else {
      if (c->getPosition().dist8(target) == 1)
        if (Creature* other = target.getCreature())
          if (other->isAffected(LastingEffect::SLEEP))
            other->removeEffect(LastingEffect::SLEEP);
      return c->moveTowards(target);
    }
  }

  virtual bool canTransfer() override {
    return !pickedUp;
  }

  SERIALIZE_ALL2(PickItem, target); 
  SERIALIZATION_CONSTRUCTOR(BringItem);

  protected:
  Position SERIAL(target);
};

PTask Task::bringItem(TaskCallback* c, Position pos, vector<Item*> items, const set<Position>& target, int numRetries) {
  return PTask(new BringItem(c, pos, items, vector<Position>(target.begin(), target.end()), numRetries));
}

class ApplyItem : public BringItem {
  public:
  ApplyItem(TaskCallback* c, Position position, vector<Item*> items, Position _target) 
      : BringItem(c, position, items, {_target}), callback(c) {}

  virtual void cancel() override {
    callback->onAppliedItemCancel(target);
  }

  virtual string getDescription() const override {
    return "Bring and apply item " + toString(position) + " to " + toString(target);
  }

  virtual CreatureAction getBroughtAction(Creature* c, vector<Item*> it) override {
    if (it.empty()) {
      cancel();
      return c->wait();
    } else {
      if (it.size() > 1)
        FAIL << it[0]->getName() << " " << it[0]->getUniqueId().getHash() << " "  << it[1]->getName() << " " <<
            it[1]->getUniqueId().getHash();
      Item* item = getOnlyElement(it);
      if (auto action = c->applyItem(item))
        return action.prepend([=](Creature* c) {
            callback->onAppliedItem(c->getPosition(), item);
          });
      else return c->wait().prepend([=](Creature* c) {
          cancel();
        });
    }
  }

  SERIALIZE_ALL2(BringItem, callback); 
  SERIALIZATION_CONSTRUCTOR(ApplyItem);

  private:
  TaskCallback* SERIAL(callback);
};

PTask Task::applyItem(TaskCallback* c, Position position, Item* item, Position target) {
  return PTask(new ApplyItem(c, position, {item}, target));
}

class ApplySquare : public Task {
  public:
  ApplySquare(TaskCallback* c, set<Position> pos, SearchType t) : positions(pos), callback(c), searchType(t) {}

  void changePosIfOccupied() {
    if (position)
      if (Creature* c = position->getCreature())
        if (!c->hasFreeMovement())
          position = none;
  }

  optional<Position> choosePosition(Creature* c) {
    vector<Position> candidates;
    for (auto& pos : positions) {
      if (Creature* other = pos.getCreature())
        if (!other->hasFreeMovement())
          continue;
      if (!rejectedPosition.count(pos))
        candidates.push_back(pos);
    }
    if (!candidates.empty())
      return chooseRandomClose(c->getPosition(), candidates, searchType);
    else
      return none;
  }

  virtual MoveInfo getMove(Creature* c) override {
    changePosIfOccupied();
    if (!position) {
      if (auto pos = choosePosition(c))
        position = pos;
      else {
        setDone();
        return NoMove;
      }
    }
    if (atTarget(c)) {
      if (auto action = c->applySquare(*position))
        return {1.0, action.append([=](Creature* c) {
            setDone();
            if (callback)
              callback->onAppliedSquare(c, *position);
        })};
      else {
        setDone();
        return NoMove;
      }
    } else {
      MoveInfo move(c->moveTowards(*position));
      if (!move || (position->dist8(c->getPosition()) == 1 && position->getCreature())) {
        rejectedPosition.insert(*position);
        position = none;
        if (--invalidCount == 0) {
          setDone();
        }
        return NoMove;
      }
      else 
        return move;
    }
  }

  virtual string getDescription() const override {
    return "Apply square " + (position ? toString(*position) : "");
  }

  bool atTarget(Creature* c) {
    return position == c->getPosition() || (!position->canEnterEmpty(c) && position->dist8(c->getPosition()) == 1);
  }

  SERIALIZE_ALL2(Task, positions, rejectedPosition, invalidCount, position, callback, searchType); 
  SERIALIZATION_CONSTRUCTOR(ApplySquare);

  private:
  set<Position> SERIAL(positions);
  set<Position> SERIAL(rejectedPosition);
  int SERIAL(invalidCount) = 5;
  optional<Position> SERIAL(position);
  TaskCallback* SERIAL(callback);
  SearchType SERIAL(searchType);
};

PTask Task::applySquare(TaskCallback* c, set<Position> position, SearchType searchType) {
  CHECK(position.size() > 0);
  return PTask(new ApplySquare(c, position, searchType));
}

namespace {

class Kill : public Task {
  public:
  enum Type { ATTACK, TORTURE };
  Kill(TaskCallback* call, Creature* c, Type t) : creature(c), type(t), callback(call) {}

  CreatureAction getAction(Creature* c) {
    switch (type) {
      case ATTACK: return c->attack(creature);
      case TORTURE: return c->torture(creature);
    }
  }

  virtual string getDescription() const override {
    switch (type) {
      case ATTACK: return "Kill " + creature->getName().bare();
      case TORTURE: return "Torture " + creature->getName().bare();
    }
    
  }

  virtual bool canPerform(const Creature* c) override {
    return c != creature;
  }

  virtual MoveInfo getMove(Creature* c) override {
    CHECK(c != creature);
    if (creature->isDead()) {
      setDone();
      return NoMove;
    }
    if (auto action = getAction(c))
      return action.append([=](Creature* c) { if (creature->isDead()) setDone(); });
    else
      return c->moveTowards(creature->getPosition());
  }

  virtual void cancel() override {
    callback->onKillCancelled(creature);
  }

  SERIALIZE_ALL2(Task, creature, type, callback); 
  SERIALIZATION_CONSTRUCTOR(Kill);

  private:
  Creature* SERIAL(creature);
  Type SERIAL(type);
  TaskCallback* SERIAL(callback);
};

}

PTask Task::kill(TaskCallback* callback, Creature* creature) {
  return PTask(new Kill(callback, creature, Kill::ATTACK));
}

PTask Task::torture(TaskCallback* callback, Creature* creature) {
  return PTask(new Kill(callback, creature, Kill::TORTURE));
}

namespace {

class DestroySquare : public Task {
  public:
  DestroySquare(Position pos) : position(pos) {
  }

  virtual MoveInfo getMove(Creature* c) override {
    if (c->getPosition().dist8(position) == 1)
      if (auto action = c->destroy(position.getDir(c->getPosition()), DestroyAction::DESTROY))
        return action.append([=](Creature* c) { 
          if (!position.canDestroy(c))
            setDone();
          });
    if (auto action = c->moveTowards(position))
      return action;
    for (Vec2 v : Vec2::directions8(Random))
      if (auto action = c->destroy(v, DestroyAction::DESTROY))
        return action;
    return NoMove;
  }

  virtual string getDescription() const override {
    return "Destroy " + toString(position);
  }

  SERIALIZE_ALL2(Task, position); 
  SERIALIZATION_CONSTRUCTOR(DestroySquare);

  private:
  Position SERIAL(position);
};

}

PTask Task::destroySquare(Position position) {
  return PTask(new DestroySquare(position));
}

namespace {

class Disappear : public Task {
  public:
  Disappear() {}

  virtual MoveInfo getMove(Creature* c) override {
    return c->disappear();
  }

  virtual string getDescription() const override {
    return "Disappear";
  }

  SERIALIZE_SUBCLASS(Task); 
};

}

PTask Task::disappear() {
  return PTask(new Disappear());
}

namespace {

class Chain : public Task {
  public:
  Chain(vector<PTask> t) : tasks(std::move(t)) {
    CHECK(!tasks.empty());
  }

  virtual bool canTransfer() override {
    return tasks.at(current)->canTransfer();
  }

  virtual void cancel() override {
    for (int i = current; i < tasks.size(); ++i)
      if (!tasks[i]->isDone())
        tasks[i]->cancel();
  }

  virtual MoveInfo getMove(Creature* c) override {
    while (tasks.at(current)->isDone())
      if (++current >= tasks.size()) {
        setDone();
        return NoMove;
      }
    return tasks[current]->getMove(c);
  }

  virtual string getDescription() const override {
    if (current >= tasks.size())
      return "Chain: done";
    return tasks[current]->getDescription();
  }

  SERIALIZE_ALL2(Task, tasks, current); 
  SERIALIZATION_CONSTRUCTOR(Chain);

  private:
  vector<PTask> SERIAL(tasks);
  int SERIAL(current) = 0;
};

}

PTask Task::chain(PTask t1, PTask t2) {
  return PTask(new Chain(makeVec<PTask>(std::move(t1), std::move(t2))));
}

PTask Task::chain(PTask t1, PTask t2, PTask t3) {
  return PTask(new Chain(makeVec<PTask>(std::move(t1), std::move(t2), std::move(t3))));
}

PTask Task::chain(vector<PTask> v) {
  return PTask(new Chain(std::move(v)));
}

namespace {

class Explore : public Task {
  public:
  Explore(Position pos) : position(pos) {}

  virtual MoveInfo getMove(Creature* c) override {
    if (!Random.roll(3))
      return NoMove;
    if (auto action = c->moveTowards(position))
      return action.append([=](Creature* c) {
          if (c->getPosition().dist8(position) < 5)
            setDone();
      });
    if (Random.roll(3))
      setDone();
    return NoMove;
  }

  virtual string getDescription() const override {
    return "Explore " + toString(position);
  }

  SERIALIZE_ALL2(Task, position); 
  SERIALIZATION_CONSTRUCTOR(Explore);

  private:
  Position SERIAL(position);
};

}

PTask Task::explore(Position pos) {
  return PTask(new Explore(pos));
}

namespace {

class AttackLeader : public Task {
  public:
  AttackLeader(Collective* col) : collective(col) {}

  virtual MoveInfo getMove(Creature* c) override {
    if (!collective->hasLeader())
      return NoMove;
    return c->moveTowards(collective->getLeader()->getPosition());
  }


  virtual string getDescription() const override {
    return "Attack " + collective->getLeader()->getName().bare();
  }
  
  SERIALIZE_ALL2(Task, collective); 
  SERIALIZATION_CONSTRUCTOR(AttackLeader);

  private:
  Collective* SERIAL(collective);
};

}

PTask Task::attackLeader(Collective* col) {
  return PTask(new AttackLeader(col));
}

PTask Task::stealFrom(Collective* collective, TaskCallback* callback) {
  vector<PTask> tasks;
  for (Position pos : collective->getConstructions().getBuiltPositions(FurnitureType::TREASURE_CHEST)) {
    vector<Item*> gold = pos.getItems(Item::classPredicate(ItemClass::GOLD));
    if (!gold.empty())
      tasks.push_back(pickItem(callback, pos, gold));
  }
  return chain(std::move(tasks));
}

namespace {

class CampAndSpawn : public Task {
  public:
  CampAndSpawn(Collective* _target, Collective* _self, CreatureFactory s, int defense, Range attack, int numAtt)
    : target(_target), self(_self), spawns(s),
      campPos(Random.permutation(target->getTerritory().getStandardExtended())), defenseSize(defense),
      attackSize(attack), numAttacks(numAtt) {}

  MoveInfo makeTeam(Creature* c, optional<TeamId>& team, int minMembers, vector<Creature*> initial, int delay) {
    if (!team || !self->getTeams().exists(*team) || self->getTeams().getMembers(*team).size() < minMembers) {
      if (!Random.roll(delay))
        return c->wait();
      return c->wait().append([this, &team, initial] (Creature* c) {
          for (Creature* spawn : Effect::summon(c->getPosition(), spawns, 1, 1000)) {
            self->addCreature(spawn, {MinionTrait::FIGHTER, MinionTrait::SUMMONED});
            if (!team || !self->getTeams().exists(*team)) {
              team = self->getTeams().createPersistent(concat(initial, {spawn}));
              self->getTeams().activate(*team);
            } else
              self->getTeams().add(*team, spawn);
          }
        });
    } else
      return NoMove;
  }

  virtual MoveInfo getMove(Creature* c) override {
    if (!target->hasLeader() || campPos.empty()) {
      setDone();
      return NoMove;
    }
    if (auto move = makeTeam(c, defenseTeam, defenseSize + 1, {c}, 1))
      return move;
    if (!contains(campPos, c->getPosition()))
      return c->moveTowards(campPos[0]);
    if (attackTeam && !self->getTeams().exists(*attackTeam))
      attackTeam.reset();
    if (!attackTeam) {
      if (numAttacks-- <= 0) {
        setDone();
        return c->wait();
      }
      currentAttack = Random.get(attackSize);
    }
    if (currentAttack) {
      if (auto move = makeTeam(c, attackTeam, *currentAttack, {}, attackTeam ? 15 : 1))
        return move;
      for (Creature* c : self->getTeams().getMembers(*attackTeam))
        self->setTask(c, Task::attackLeader(target));
      currentAttack.reset();
    }
    return c->wait();
  }

  virtual string getDescription() const override {
    return "Camp and spawn " + target->getLeader()->getName().bare();
  }
 
  SERIALIZE_ALL2(Task, target, self, spawns, campPos, defenseSize, attackSize, currentAttack, defenseTeam, attackTeam, numAttacks); 
  SERIALIZATION_CONSTRUCTOR(CampAndSpawn);

  private:
  Collective* SERIAL(target);
  Collective* SERIAL(self);
  CreatureFactory SERIAL(spawns);
  vector<Position> SERIAL(campPos);
  int SERIAL(defenseSize);
  Range SERIAL(attackSize);
  optional<int> SERIAL(currentAttack);
  optional<TeamId> SERIAL(defenseTeam);
  optional<TeamId> SERIAL(attackTeam);
  int SERIAL(numAttacks);
};


}

PTask Task::campAndSpawn(Collective* target, Collective* self, const CreatureFactory& spawns, int defenseSize,
    Range attackSize, int numAttacks) {
  return PTask(new CampAndSpawn(target, self, spawns, defenseSize, attackSize, numAttacks));
}

namespace {

class KillFighters : public Task {
  public:
  KillFighters(Collective* col, int numC) : collective(col), numCreatures(numC) {}

  virtual MoveInfo getMove(Creature* c) override {
    for (const Creature* target : collective->getCreatures(MinionTrait::FIGHTER))
      targets.insert(target);
    int numKilled = 0;
    for (Creature::Id victim : c->getKills())
      if (targets.contains(victim))
        ++numKilled;
    if (numKilled >= numCreatures || targets.empty()) {
      setDone();
      return NoMove;
    }
    return c->moveTowards(collective->getLeader()->getPosition());
  }

  virtual string getDescription() const override {
    return "Kill " + toString(numCreatures) + " minions of " + collective->getName().getFull();
  }

  SERIALIZE_ALL2(Task, collective, numCreatures, targets); 
  SERIALIZATION_CONSTRUCTOR(KillFighters);

  private:
  Collective* SERIAL(collective);
  int SERIAL(numCreatures);
  EntitySet<Creature> SERIAL(targets);
};

}

PTask Task::killFighters(Collective* col, int numCreatures) {
  return PTask(new KillFighters(col, numCreatures));
}

namespace {
class ConsumeItem : public Task {
  public:
  ConsumeItem(TaskCallback* c, vector<Item*> _items) : items(_items), callback(c) {}

  virtual MoveInfo getMove(Creature* c) override {
    return c->wait().append([=](Creature* c) {
        c->getEquipment().removeItems(c->getEquipment().getItems(items.containsPredicate())); });
  }

  virtual string getDescription() const override {
    return "Consume item";
  }
  
  SERIALIZE_ALL2(Task, items, callback); 
  SERIALIZATION_CONSTRUCTOR(ConsumeItem);

  protected:
  EntitySet<Item> SERIAL(items);
  TaskCallback* SERIAL(callback);
};
}

PTask Task::consumeItem(TaskCallback* c, vector<Item*> items) {
  return PTask(new ConsumeItem(c, items));
}

namespace {
class Copulate : public Task {
  public:
  Copulate(TaskCallback* c, Creature* t, int turns) : target(t), callback(c), numTurns(turns) {}

  virtual MoveInfo getMove(Creature* c) override {
    if (target->isDead() || !target->isAffected(LastingEffect::SLEEP)) {
      setDone();
      return NoMove;
    }
    if (c->getPosition().dist8(target->getPosition()) == 1) {
      if (Random.roll(2))
        for (Vec2 v : Vec2::directions8(Random))
          if (v.dist8(c->getPosition().getDir(target->getPosition())) == 1)
            if (auto action = c->move(v))
              return action;
      if (auto action = c->copulate(c->getPosition().getDir(target->getPosition())))
        return action.append([=](Creature* c) {
          if (--numTurns == 0) {
            setDone();
            callback->onCopulated(c, target);
          }});
      else
        return NoMove;
    } else
      return c->moveTowards(target->getPosition());
  }

  virtual string getDescription() const override {
    return "Copulate with " + target->getName().bare();
  }

  SERIALIZE_ALL2(Task, target, numTurns, callback); 
  SERIALIZATION_CONSTRUCTOR(Copulate);

  protected:
  Creature* SERIAL(target);
  TaskCallback* SERIAL(callback);
  int SERIAL(numTurns);
};
}

PTask Task::copulate(TaskCallback* c, Creature* target, int numTurns) {
  return PTask(new Copulate(c, target, numTurns));
}

namespace {
class Consume : public Task {
  public:
  Consume(TaskCallback* c, Creature* t) : target(t), callback(c) {}

  virtual MoveInfo getMove(Creature* c) override {
    if (target->isDead()) {
      setDone();
      return NoMove;
    }
    if (c->getPosition().dist8(target->getPosition()) == 1) {
      if (auto action = c->consume(target))
        return action.append([=](Creature* c) {
          setDone();
          callback->onConsumed(c, target);
        });
      else
        return NoMove;
    } else
      return c->moveTowards(target->getPosition());
  }

  virtual string getDescription() const override {
    return "Absorb " + target->getName().bare();
  }

  SERIALIZE_ALL2(Task, target, callback); 
  SERIALIZATION_CONSTRUCTOR(Consume);

  protected:
  Creature* SERIAL(target);
  TaskCallback* SERIAL(callback);
};
}

PTask Task::consume(TaskCallback* c, Creature* target) {
  return PTask(new Consume(c, target));
}

namespace {

class Eat : public Task {
  public:
  Eat(set<Position> pos) : positions(pos) {}

  virtual bool canTransfer() override {
    return false;
  }

  Item* getDeadChicken(Position pos) {
    vector<Item*> chickens = pos.getItems(Item::classPredicate(ItemClass::FOOD));
    if (chickens.empty())
      return nullptr;
    else
      return chickens[0];
  }

  virtual MoveInfo getMove(Creature* c) override {
    if (!position) {
      for (Position v : positions)
        if (!rejectedPosition.count(v) && (!position ||
              position->dist8(c->getPosition()) > v.dist8(c->getPosition())))
          position = v;
      if (!position) {
        setDone();
        return NoMove;
      }
    }
    if (c->getPosition() != *position && getDeadChicken(*position))
      return c->moveTowards(*position);
    Item* chicken = getDeadChicken(c->getPosition());
    if (chicken)
      return c->eat(chicken).append([=] (Creature* c) {
        setDone();
      });
    for (Position pos : c->getPosition().neighbors8(Random)) {
      Item* chicken = getDeadChicken(pos);
      if (chicken) 
        if (auto move = c->move(pos))
          return move;
      if (Creature* ch = pos.getCreature())
        if (ch->getBody().isMinionFood())
          if (auto move = c->attack(ch)) {
            return move.append([this, ch, pos] (Creature*) { if (ch->isDead()) position = pos; });
      }
    }
    return c->moveTowards(*position);
  }

  virtual string getDescription() const override {
    if (position)
      return "Eat at " + toString(*position);
    else
      return "Eat";
  }

  SERIALIZE_ALL2(Task, position, positions, rejectedPosition); 
  SERIALIZATION_CONSTRUCTOR(Eat);

  private:
  optional<Position> SERIAL(position);
  set<Position> SERIAL(positions);
  set<Position> SERIAL(rejectedPosition);
};

}

PTask Task::eat(set<Position> hatcherySquares) {
  return PTask(new Eat(hatcherySquares));
}

namespace {
class GoTo : public Task {
  public:
  GoTo(Position pos, bool forever) : target(pos), tryForever(forever) {}

  virtual MoveInfo getMove(Creature* c) override {
    if (c->getPosition() == target ||
        (c->getPosition().dist8(target) == 1 && !target.canEnterEmpty(c)) ||
        (!tryForever && !c->canNavigateTo(target))) {
      setDone();
      return NoMove;
    } else
      return c->moveTowards(target);
  }

  virtual string getDescription() const override {
    return "Go to " + toString(target);
  }

  SERIALIZE_ALL2(Task, target, tryForever)
  SERIALIZATION_CONSTRUCTOR(GoTo);

  protected:
  Position SERIAL(target);
  bool SERIAL(tryForever);
};
}

PTask Task::goTo(Position pos) {
  return PTask(new GoTo(pos, false));
}

PTask Task::goToTryForever(Position pos) {
  return PTask(new GoTo(pos, true));
}

namespace {
class TransferTo : public Task {
  public:
  TransferTo(Model* m) : model(m) {}

  virtual MoveInfo getMove(Creature* c) override {
    if (!target)
      target = c->getGame()->getTransferPos(model, c->getPosition().getModel());
    if (c->getPosition() == target && c->getGame()->canTransferCreature(c, model)) {
      return c->wait().append([=] (Creature* c) { setDone(); c->getGame()->transferCreature(c, model); });
    } else
      return c->moveTowards(*target);
  }

  virtual string getDescription() const override {
    return "Go to site";
  }

  SERIALIZE_ALL2(Task, target, model); 
  SERIALIZATION_CONSTRUCTOR(TransferTo);

  protected:
  optional<Position> SERIAL(target);
  Model* SERIAL(model);
};
}

PTask Task::transferTo(Model *m) {
  return PTask(new TransferTo(m));
}

namespace {
class GoToAndWait : public Task {
  public:
  GoToAndWait(Position pos, double waitT) : position(pos), waitTime(waitT) {}

  bool arrived(Creature* c) {
    return c->getPosition() == position ||
        (c->getPosition().dist8(position) == 1 && !position.canEnterEmpty(c));
  }

  virtual MoveInfo getMove(Creature* c) override {
    if (maxTime && c->getLocalTime() >= *maxTime) {
      setDone();
      return NoMove;
    }
    if (!arrived(c)) {
      auto ret = c->moveTowards(position);
      if (!ret) {
        if (!timeout)
          timeout = c->getLocalTime() + 30;
        else
          if (c->getLocalTime() > *timeout) {
            setDone();
            return NoMove;
          }
      } else
        timeout = none;
      return ret;
    } else {
      if (!maxTime)
        maxTime = c->getLocalTime() + waitTime;
      return c->wait();
    }
  }

  virtual string getDescription() const override {
    return "Go to and wait " + toString(position);
  }

  SERIALIZE_ALL2(Task, position, waitTime, maxTime, timeout);
  SERIALIZATION_CONSTRUCTOR(GoToAndWait);

  private:
  Position SERIAL(position);
  double SERIAL(waitTime);
  optional<double> SERIAL(maxTime);
  optional<double> SERIAL(timeout);
};
}

PTask Task::goToAndWait(Position pos, double waitTime) {
  return PTask(new GoToAndWait(pos, waitTime));
}

namespace {
class Whipping : public Task {
  public:
  Whipping(Position pos, Creature* w) : position(pos), whipped(w) {}

  virtual bool canPerform(const Creature* c) override {
    return c != whipped;
  }

  virtual MoveInfo getMove(Creature* c) override {
    if (position.getCreature() != whipped || !whipped->isAffected(LastingEffect::TIED_UP)) {
      setDone();
      return NoMove;
    }
    if (c->getPosition().dist8(position) > 1)
      return c->moveTowards(position);
    else
      return c->whip(whipped->getPosition());
  }

  virtual string getDescription() const override {
    return "Whipping " + whipped->getName().a();
  }

  SERIALIZE_ALL2(Task, position, whipped);
  SERIALIZATION_CONSTRUCTOR(Whipping);

  protected:
  Position SERIAL(position);
  Creature* SERIAL(whipped);
};
}

PTask Task::whipping(Position pos, Creature* whipped) {
  return PTask(new Whipping(pos, whipped));
}

namespace {
class DropItems : public Task {
  public:
  DropItems(EntitySet<Item> it) : items(it) {}

  virtual MoveInfo getMove(Creature* c) override {
    return c->drop(c->getEquipment().getItems(items.containsPredicate())).append([=] (Creature*) { setDone(); });
  }

  virtual string getDescription() const override {
    return "Drop items";
  }

  SERIALIZE_ALL2(Task, items); 
  SERIALIZATION_CONSTRUCTOR(DropItems);

  protected:
  EntitySet<Item> SERIAL(items);
};
}

PTask Task::dropItems(vector<Item*> items) {
  return PTask(new DropItems(items));
}

namespace {
class Spider : public Task {
  public:
  Spider(Position orig, const vector<Position>& pos, const vector<Position>& pos2)
      : origin(orig), positionsClose(pos), positionsFurther(pos2) {}

  virtual MoveInfo getMove(Creature* c) override {
    if (Random.roll(10))
      for (auto& pos : Random.permutation(positionsFurther))
        if (pos.getCreature() && pos.getCreature()->isAffected(LastingEffect::ENTANGLED)) {
          makeWeb = pos;
          break;
        }
    if (!makeWeb && Random.roll(10)) {
      vector<Position>& positions = Random.roll(10) ? positionsFurther : positionsClose;
      for (auto& pos : Random.permutation(positions))
        if (pos.getTriggers().empty() && !!c->moveTowards(pos, true)) {
          makeWeb = pos;
          break;
        }
    }
    if (!makeWeb)
      return c->moveTowards(origin);
    if (c->getPosition() == *makeWeb)
      return c->wait().append([this](Creature* c) {
            ItemFactory::fromId(ItemType{ItemId::TRAP_ITEM,
                TrapInfo{TrapType::WEB, EffectType{EffectId::LASTING, LastingEffect::ENTANGLED}, true}})->
                apply(c, true);
            setDone();
          });
    else
      return c->moveTowards(*makeWeb, true);
  }

  virtual string getDescription() const override {
    return "Spider";
  }

  SERIALIZE_ALL2(Task, origin, positionsClose, positionsFurther, makeWeb); 
  SERIALIZATION_CONSTRUCTOR(Spider);

  protected:
  Position SERIAL(origin);
  vector<Position> SERIAL(positionsClose);
  vector<Position> SERIAL(positionsFurther);
  optional<Position> SERIAL(makeWeb);
};
}

PTask Task::spider(Position origin, const vector<Position>& posClose, const vector<Position>& posFurther) {
  return PTask(new Spider(origin, posClose, posFurther));
}

template <class Archive>
void Task::registerTypes(Archive& ar, int version) {
  REGISTER_TYPE(ar, Construction);
  REGISTER_TYPE(ar, Destruction);
  REGISTER_TYPE(ar, BuildTorch);
  REGISTER_TYPE(ar, PickItem);
  REGISTER_TYPE(ar, PickAndEquipItem);
  REGISTER_TYPE(ar, EquipItem);
  REGISTER_TYPE(ar, BringItem);
  REGISTER_TYPE(ar, ApplyItem);
  REGISTER_TYPE(ar, ApplySquare);
  REGISTER_TYPE(ar, Kill);
  REGISTER_TYPE(ar, DestroySquare);
  REGISTER_TYPE(ar, Disappear);
  REGISTER_TYPE(ar, Chain);
  REGISTER_TYPE(ar, Explore);
  REGISTER_TYPE(ar, AttackLeader);
  REGISTER_TYPE(ar, KillFighters);
  REGISTER_TYPE(ar, ConsumeItem);
  REGISTER_TYPE(ar, Copulate);
  REGISTER_TYPE(ar, Consume);
  REGISTER_TYPE(ar, Eat);
  REGISTER_TYPE(ar, GoTo);
  REGISTER_TYPE(ar, TransferTo);
  REGISTER_TYPE(ar, Whipping);
  REGISTER_TYPE(ar, GoToAndWait);
  REGISTER_TYPE(ar, DropItems);
  REGISTER_TYPE(ar, CampAndSpawn);
  REGISTER_TYPE(ar, Spider);
}

REGISTER_TYPES(Task::registerTypes);
