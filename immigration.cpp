#include "stdafx.h"
#include "immigration.h"
#include "collective.h"
#include "collective_config.h"
#include "monster_ai.h"
#include "creature.h"
#include "creature_attributes.h"
#include "territory.h"
#include "game.h"
#include "furniture_factory.h"
#include "construction_map.h"
#include "collective_teams.h"
#include "furniture.h"
#include "item_index.h"
#include "technology.h"
#include "level.h"
#include "game.h"
#include "collective_name.h"

SERIALIZE_DEF(Immigration, immigrants, available, minionAttraction, idCnt, collective, generated, candidateTimeout, initialized)

SERIALIZATION_CONSTRUCTOR_IMPL(Immigration)

static map<AttractionType, int> empty;

int Immigration::getAttractionOccupation(const AttractionType& type) const {
  int res = 0;
  for (auto creature : collective->getCreatures()) {
    auto& elems = minionAttraction.getOrElse(creature, empty);
    auto it = elems.find(type);
    if (it != elems.end())
      res += it->second;
  }
  return res;
}

int Immigration::getAttractionValue(const AttractionType& attraction) const {
  return apply_visitor(attraction, makeVisitor<int>(
        [&](FurnitureType type) {
          auto& constructions = collective->getConstructions();
          int ret = constructions.getBuiltCount(type);
          for (auto upgrade : FurnitureFactory::getUpgrades(type))
            ret += constructions.getBuiltCount(upgrade);
          return ret;
        },
        [&](ItemIndex index) {
          return collective->getNumItems(index, false);
        }));
}

static void visitAttraction(const Immigration& immigration, const AttractionInfo& attraction,
    function<void(int total, int available)> visit) {
  int value = 0;
  int occupation = 0;
  for (auto& type : attraction.types) {
    int thisValue = immigration.getAttractionValue(type);
    value += thisValue;
    occupation += min(thisValue, immigration.getAttractionOccupation(type));
  }
  visit(value, max(0, value - occupation));
}

vector<string> Immigration::getMissingRequirements(const Available& available) const {
  vector<string> ret = getMissingRequirements(Group {available.immigrantIndex, (int)available.getCreatures().size()});
  int groupSize = available.getCreatures().size();
  if (!available.getInfo().getTraits().contains(MinionTrait::NO_LIMIT) &&
      groupSize > collective->getMaxPopulation() - collective->getPopulationSize())
    ret.push_back("Exceeds population limit");
  auto spawnType = available.getCreatures()[0]->getAttributes().getSpawnType();
  if (ret.empty() && available.getSpawnPositions().size() < groupSize)
    ret.push_back("Not enough room to spawn.");
  return ret;
}

vector<string> Immigration::getMissingRequirements(const Group& group) const {
  vector<string> ret;
  auto& immigrantInfo = immigrants[group.immigrantIndex];
  auto visitor = makeVisitor<void>(
      [&](const AttractionInfo& attraction) {
        return visitAttraction(*this, attraction,
            [&](int total, int available) {
              int required = attraction.amountClaimed * group.count - available;
              if (required > 0) {
                const char* extra = total > 0 ? "more " : "";
                ret.push_back("Requires " + toString(required) + " " + extra +
                    combineWithOr(transform2<string>(attraction.types,
                        [&](const AttractionType& type) { return AttractionInfo::getAttractionName(type, required); })));
              }
            });
      },
      [&](const TechId& techId) {
        if (!collective->hasTech(techId))
          ret.push_back("Missing technology: " + Technology::get(techId)->getName());
      },
      [&](const SunlightState& state) {
        if (state != collective->getGame()->getSunlightInfo().getState())
          ret.push_back("Immigrant won't join during the "_s + collective->getGame()->getSunlightInfo().getText());
      },
      [&](const CostInfo& cost) {
        if (!collective->hasResource(cost * group.count))
          ret.push_back("Not enough " + CollectiveConfig::getResourceInfo(cost.id).name);
      },
      [&](const ExponentialCost& cost) {
        if (!collective->hasResource(calculateCost(group.immigrantIndex, cost) * group.count))
          ret.push_back("Not enough " + CollectiveConfig::getResourceInfo(cost.base.id).name);
      },
      [&](const FurnitureType& type) {
        if (collective->getConstructions().getBuiltCount(type) == 0)
          ret.push_back("Requires " + Furniture::getName(type));
      },
      [&](const Pregnancy&) {
        for (Creature* c : collective->getCreatures())
          if (c->isAffected(LastingEffect::PREGNANT))
            return;
        ret.push_back("Requires a pregnant succubus");
      },
      [&](const RecruitmentInfo& info) {
        auto col = info.findEnemy(collective->getGame());
        if (!col)
          ret.push_back("Ally doesn't exist.");
        if (!collective->isKnownVillainLocation(col))
          ret.push_back("Ally hasn't been discovered.");
        else if (info.getRecruits(collective->getGame(), immigrantInfo.getId(0)).empty())
          ret.push_back(col->getName().getFull() + " don't have recruits available at this moment.");
      }
  );
  immigrantInfo.visitRequirements(visitor);
  return ret;
}

bool Immigration::preliminaryRequirementsMet(const Group& group) const {
  bool ret = true;
  auto visitor = makeVisitor<void>(
      [&](const AttractionInfo& attraction) {
        visitAttraction(*this, attraction,
            [&](int, int available) { return ret &= (available > 0); });
      },
      [&](const TechId& techId) {
        ret &= collective->hasTech(techId);
      },
      [&](const SunlightState& state) {
        ret &= (state == collective->getGame()->getSunlightInfo().getState());
      },
      [&](const FurnitureType& type) {
        ret &= (collective->getConstructions().getBuiltCount(type) > 0);
      },
      [&](const CostInfo& cost) {
        ret &= collective->hasResource(cost * group.count);
      },
      [&](const ExponentialCost& cost) {
        ret &= collective->hasResource(calculateCost(group.immigrantIndex, cost) * group.count);
      },
      [&](const Pregnancy&) {
        for (Creature* c : collective->getCreatures())
          if (c->isAffected(LastingEffect::PREGNANT))
            return;
        ret = false;
      },
      [&](const RecruitmentInfo& info) {
        Collective* col = info.findEnemy(collective->getGame());
        if (!col || !collective->isKnownVillainLocation(col) ||
            info.getRecruits(collective->getGame(), immigrants[group.immigrantIndex].getId(0)).empty())
          ret = false;
      }
  );
  immigrants[group.immigrantIndex].visitPreliminaryRequirements(visitor);
  return ret;
}

void Immigration::occupyRequirements(const Creature* c, int index) {
  auto visitor = makeVisitor<void>(
      [&](const AttractionInfo& attraction) { occupyAttraction(c, attraction); },
      [&](const TechId&) {},
      [&](const SunlightState&) {},
      [&](const FurnitureType&) {},
      [&](const CostInfo& cost) {
        collective->takeResource(cost);
      },
      [&](const ExponentialCost& cost) {
        collective->takeResource(calculateCost(index, cost));
      },
      [&](const Pregnancy&) {
        for (Creature* c : collective->getCreatures())
          if (c->isAffected(LastingEffect::PREGNANT)) {
            c->removeEffect(LastingEffect::PREGNANT);
            break;
          }
      },
      [&](const RecruitmentInfo&) {}
  );
  immigrants[index].visitRequirements(visitor);
}

void Immigration::occupyAttraction(const Creature* c, const AttractionInfo& attraction) {
  int toOccupy = attraction.amountClaimed;
  vector<AttractionType> occupied;
  for (auto& type : attraction.types) {
    int nowOccupy = min(toOccupy, max(0,
        getAttractionValue(type) - getAttractionOccupation(type)));
    if (nowOccupy > 0) {
      CHECK(nowOccupy <= toOccupy) << nowOccupy << " " << toOccupy;
      toOccupy -= nowOccupy;
      minionAttraction.getOrInit(c)[type] += nowOccupy;
    }
    if (toOccupy == 0)
      break;
  }
  CHECK(toOccupy == 0) << toOccupy;
}

double Immigration::getImmigrantChance(const Group& group) const {
  auto& info = immigrants[group.immigrantIndex];
  if (info.isPersistent() || !preliminaryRequirementsMet(group))
    return 0.0;
  else
    return info.getFrequency();
}

Immigration::Immigration(Collective* c)
    : immigrants(c->getConfig().getImmigrantInfo()), collective(c),
      candidateTimeout(c->getConfig().getImmigrantTimeout()) {
}

map<int, std::reference_wrapper<const Immigration::Available>> Immigration::getAvailable() const {
  map<int, std::reference_wrapper<const Immigration::Available>> ret;
  for (auto& elem : available)
    if (!elem.second.isUnavailable() &&
        preliminaryRequirementsMet({elem.second.immigrantIndex, (int)elem.second.getCreatures().size()}))
      ret.emplace(elem.first, std::cref(elem.second));
  return ret;
}

static vector<Position> pickSpawnPositions(const vector<Creature*>& creatures, vector<Position> allPositions) {
  if (allPositions.empty() || creatures.empty())
    return {};
  for (auto& pos : copyOf(allPositions))
    if (!pos.canEnter(creatures[0]))
      for (auto& neighbor : pos.neighbors8())
        if (neighbor.canEnter(creatures[0]))
          allPositions.push_back(neighbor);
  vector<Position> spawnPos;
  for (auto c : creatures) {
    Position pos;
    int cnt = 100;
    do {
      pos = Random.choose(allPositions);
    } while ((!pos.canEnter(c) || contains(spawnPos, pos)) && --cnt > 0);
    if (cnt == 0) {
      INFO << "Couldn't spawn immigrant " << c->getName().bare();
      return {};
    } else
      spawnPos.push_back(pos);
  }
  return spawnPos;
}

vector<Position> Immigration::Available::getSpawnPositions() const {
  vector<Position> positions = apply_visitor(getInfo().getSpawnLocation(), makeVisitor<vector<Position>>(
    [&] (FurnitureType type) {
      return asVector<Position>(immigration->collective->getConstructions().getBuiltPositions(type));
    },
    [&] (OutsideTerritory) {
      auto ret = immigration->collective->getTerritory().getExtended(10, 20);
      if (ret.empty())
        ret = immigration->collective->getTerritory().getAll();
      if (ret.empty() && immigration->collective->hasLeader())
        ret = {immigration->collective->getLeader()->getPosition()};
      return ret;
    },
    [&] (NearLeader) {
      return vector<Position> { immigration->collective->getLeader()->getPosition() };
    },
    [&] (Pregnancy) {
      vector<Position> ret;
      for (Creature* c : immigration->collective->getCreatures())
        if (c->isAffected(LastingEffect::PREGNANT))
          ret.push_back(c->getPosition());
      return ret;
    }
  ));
  return pickSpawnPositions(getCreatures(), positions);
}

const ImmigrantInfo& Immigration::Available::getInfo() const {
  return immigration->immigrants[immigrantIndex];
}

optional<double> Immigration::Available::getEndTime() const {
  return endTime;
}

optional<CostInfo> Immigration::Available::getCost() const {
  optional<CostInfo> ret;
  auto visitor = makeDefaultVisitor(
      [&](const CostInfo& cost) {
        ret = cost;
      },
      [&](const ExponentialCost& cost) {
        ret = immigration->calculateCost(immigrantIndex, cost);
      }
  );
  getInfo().visitRequirements(visitor);
  return ret;
}

CostInfo Immigration::calculateCost(int index, const ExponentialCost& cost) const {
  EntitySet<Creature> existing;
  for (Creature* c : collective->getCreatures())
    existing.insert(c);
  int numType = 0;
  if (auto gen = getMaybe(generated, index))
    for (Creature* c : *gen)
      if (existing.contains(c))
        ++numType;
  if (numType < cost.numFree)
    return CostInfo::noCost();
  else
    return CostInfo(cost.base.id, cost.base.value * pow(2, double(numType - cost.numFree) / cost.numToDoubleCost));
}

/*const ImmigrantInfo& Immigration::Available::getImmigrantInfo() const {
  return info;
}*/

Immigration::Available::Available(Immigration* im, Creatures c, int ind, optional<double> t)
  : creatures(std::move(c)), immigrantIndex(ind), endTime(t), immigration(im) {
}

void Immigration::Available::addAllCreatures(const vector<Position>& spawnPositions) {
  apply_visitor(creatures, makeVisitor<void>(
      [&](vector<PCreature>& creatures) {
        for (auto creature : Iter(creatures))
          immigration->collective->addCreature(std::move(*creature), spawnPositions[creature.index()],
              getInfo().getTraits());
      },
      [&](vector<Creature*>& creatures) {
        for (Creature* c : creatures) {
          immigration->collective->addCreature(c, {MinionTrait::FIGHTER});
          Model* target = immigration->collective->getLevel()->getModel();
          if (c->getPosition().getModel() != target)
            c->getGame()->transferCreature(c, target);
        }
      }
  ));
}

void Immigration::accept(int id) {
  if (!available.count(id))
    return;
  auto& candidate = available.at(id);
  auto& immigrantInfo = immigrants[candidate.immigrantIndex];
  if (!getMissingRequirements(candidate).empty())
    return;
  vector<Creature*> creatures = candidate.getCreatures();
  const int groupSize = creatures.size();
  vector<Position> spawnPos = candidate.getSpawnPositions();
  if (spawnPos.size() < groupSize)
    return;
  if (immigrantInfo.isAutoTeam() && groupSize > 1)
    collective->getTeams().activate(collective->getTeams().createPersistent(creatures));
  collective->addNewCreatureMessage(creatures);
  for (int i : All(creatures)) {
    Creature* c = creatures[i];
    if (i == 0 && groupSize > 1) // group leader
      c->getAttributes().increaseBaseExpLevel(2);
    occupyRequirements(c, candidate.immigrantIndex);
    generated[candidate.immigrantIndex].push_back(c);
  }
  candidate.addAllCreatures(spawnPos);
  if (!immigrantInfo.isPersistent())
    reject(id);
  else if (immigrantInfo.isAvailable(generated[candidate.immigrantIndex].size()))
    available[id] = Available::generate(this, candidate.immigrantIndex);
}

void Immigration::reject(int id) {
  if (auto immigrant = getMaybe(available, id))
    if (!immigrant->getInfo().isPersistent())
      immigrant->endTime = -1;
}

SERIALIZE_DEF(Immigration::Available, creatures, immigrantIndex, endTime, immigration)
SERIALIZATION_CONSTRUCTOR_IMPL2(Immigration::Available, Available)

Immigration::Available Immigration::Available::generate(Immigration* immigration, int index) {
  return generate(immigration, Group {index, Random.get(immigration->immigrants[index].getGroupSize()) });
}

Immigration::Available Immigration::Available::generate(Immigration* immigration, const Group& group) {
  ImmigrantInfo& info = immigration->immigrants[group.immigrantIndex];
  optional<Creatures> creatures;
  info.visitRequirements(makeDefaultVisitor(
      [&](const RecruitmentInfo& info) {
        if (Collective* col = info.findEnemy(immigration->collective->getGame()))
          creatures = Creatures(makeVec<Creature*>(col->getCreatures(info.trait).at(0) ));
      }));
  if (!creatures) {
    vector<PCreature> immigrants;
    int numGenerated = immigration->generated[group.immigrantIndex].size();
    for (int i : Range(group.count))
      immigrants.push_back(CreatureFactory::fromId(info.getId(numGenerated), immigration->collective->getTribeId(),
          MonsterAIFactory::collective(immigration->collective)));
    creatures = std::move(immigrants);
  }
  return Available(
    immigration,
    std::move(*creatures),
    group.immigrantIndex,
    !info.isPersistent()
        ? immigration->collective->getGame()->getGlobalTime() + immigration->candidateTimeout
        : optional<double>(none)
  );
}

vector<Creature*> Immigration::Available::getCreatures() const {
  return apply_visitor(creatures, makeVisitor<vector<Creature*>>(
          [](const vector<Creature*>& v) { return v;},
          [](const vector<PCreature>& v) { return extractRefs(v);}
      ));
}

bool Immigration::Available::isUnavailable() const {
  return creatures.empty() || (endTime && endTime < immigration->collective->getGame()->getGlobalTime());
}

void Immigration::initializePersistent() {
  for (auto elem : Iter(immigrants))
    if (elem->isPersistent()) {
      available.emplace(++idCnt, Available::generate(this, Group{elem.index(), 1}));
      auto& elem = available.at(idCnt);
      for (int i : Range(elem.getInfo().getInitialRecruitment()))
        accept(idCnt);
    }
}

void Immigration::update() {
  if (!initialized) {
    initialized = true;
    initializePersistent();
  }
  for (auto elem : Iter(available))
    if (elem->second.isUnavailable())
      elem.markToErase();
  if (Random.chance(collective->getConfig().getImmigrantFrequency())) {
    vector<Group> immigrantInfo;
    for (auto elem : Iter(immigrants))
      immigrantInfo.push_back(Group {elem.index(), Random.get(elem->getGroupSize())});
    vector<double> weights = transform2<double>(immigrantInfo,
        [&](const Group& group) { return getImmigrantChance(group);});
    if (std::accumulate(weights.begin(), weights.end(), 0.0) > 0)
      available.emplace(++idCnt, Available::generate(this, Random.choose(immigrantInfo, weights)));
  }
}
