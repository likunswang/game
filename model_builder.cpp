#include "stdafx.h"
#include "model_builder.h"
#include "level.h"
#include "tribe.h"
#include "item_type.h"
#include "inventory.h"
#include "collective_builder.h"
#include "options.h"
#include "player_control.h"
#include "spectator.h"
#include "creature.h"
#include "square.h"
#include "progress_meter.h"
#include "collective.h"
#include "level_maker.h"
#include "model.h"
#include "level_builder.h"
#include "monster_ai.h"
#include "game.h"
#include "campaign.h"
#include "creature_name.h"
#include "villain_type.h"
#include "enemy_factory.h"
#include "location.h"
#include "event_proxy.h"
#include "view_object.h"
#include "item.h"
#include "furniture.h"
#include "sokoban_input.h"
#include "external_enemies.h"
#include "immigration.h"
#include "technology.h"

using namespace std::chrono;

ModelBuilder::ModelBuilder(ProgressMeter* m, RandomGen& r, Options* o, SokobanInput* sok) : random(r), meter(m), options(o),
  enemyFactory(EnemyFactory(random)), sokobanInput(sok) {
}

ModelBuilder::~ModelBuilder() {
}

int ModelBuilder::getPigstyPopulationIncrease() {
  return 4;
}

int ModelBuilder::getStatuePopulationIncrease() {
  return 1;
}

int ModelBuilder::getThronePopulationIncrease() {
  return 10;
}

static CollectiveConfig getKeeperConfig(bool fastImmigration) {
  return CollectiveConfig::keeper(
      fastImmigration ? 0.1 : 0.007,
      500,
      2,
      10,
      {
      CONSTRUCT(PopulationIncrease,
        c.type = FurnitureType::PIGSTY;
        c.increasePerSquare = 0.25;
        c.maxIncrease = ModelBuilder::getPigstyPopulationIncrease();),
      CONSTRUCT(PopulationIncrease,
        c.type = FurnitureType::MINION_STATUE;
        c.increasePerSquare = ModelBuilder::getStatuePopulationIncrease();
        c.maxIncrease = 1000;),
      CONSTRUCT(PopulationIncrease,
        c.type = FurnitureType::THRONE;
        c.increasePerSquare = ModelBuilder::getThronePopulationIncrease();
        c.maxIncrease = c.increasePerSquare;),
      },
      {
      ImmigrantInfo(CreatureId::IMP, {MinionTrait::WORKER, MinionTrait::NO_LIMIT, MinionTrait::NO_EQUIPMENT})
          .setSpawnLocation(NearLeader{})
          .setInitialRecruitment(4)
          .addRequirement(ExponentialCost{ CostInfo(CollectiveResourceId::MANA, 20), 5, 4 }),
      ImmigrantInfo(CreatureId::GOBLIN, {MinionTrait::FIGHTER, MinionTrait::NO_EQUIPMENT})
          .setFrequency(0.7)
          .addRequirement(AttractionInfo{1, vector<AttractionType>(
               {FurnitureType::FORGE, FurnitureType::WORKSHOP, FurnitureType::JEWELER})}),
      ImmigrantInfo(CreatureId::ORC, {MinionTrait::FIGHTER})
          .setFrequency(0.7)
          .addRequirement(AttractionInfo{1, FurnitureType::TRAINING_WOOD}),
      ImmigrantInfo(CreatureId::ORC_SHAMAN, {MinionTrait::FIGHTER})
          .setFrequency(0.6)
          .addRequirement(AttractionInfo{1, {FurnitureType::BOOK_SHELF, FurnitureType::LABORATORY}}),
      ImmigrantInfo(CreatureId::OGRE, {MinionTrait::FIGHTER})
          .setFrequency(0.3)
          .addRequirement(AttractionInfo{1, FurnitureType::TRAINING_IRON}),
      ImmigrantInfo(CreatureId::HARPY, {MinionTrait::FIGHTER})
          .setFrequency(0.3)
          .addRequirement(AttractionInfo{1, FurnitureType::TRAINING_WOOD})
          .addRequirement(AttractionInfo{1, ItemIndex::RANGED_WEAPON}),
      ImmigrantInfo(CreatureId::ZOMBIE, {MinionTrait::FIGHTER})
          .setFrequency(0.5)
          .setSpawnLocation(FurnitureType::GRAVE)
          .addPreliminaryRequirement(CostInfo(CollectiveResourceId::CORPSE, 1)),
      ImmigrantInfo(CreatureId::VAMPIRE, {MinionTrait::FIGHTER})
          .setFrequency(0.2)
          .setSpawnLocation(FurnitureType::GRAVE)
          .addRequirement(AttractionInfo{1, FurnitureType::TRAINING_IRON})
          .addPreliminaryRequirement(CostInfo(CollectiveResourceId::CORPSE, 1)),
      ImmigrantInfo(CreatureId::LOST_SOUL, {MinionTrait::FIGHTER})
          .setFrequency(0.3)
          .setSpawnLocation(FurnitureType::DEMON_SHRINE)
          .addRequirement(AttractionInfo{1, FurnitureType::DEMON_SHRINE})
          .addPreliminaryRequirement(FurnitureType::DEMON_SHRINE),
      ImmigrantInfo(CreatureId::SUCCUBUS, {MinionTrait::FIGHTER, MinionTrait::NO_EQUIPMENT})
          .setFrequency(0.3)
          .setSpawnLocation(FurnitureType::DEMON_SHRINE)
          .addRequirement(AttractionInfo{2, FurnitureType::DEMON_SHRINE})
          .addPreliminaryRequirement(FurnitureType::DEMON_SHRINE),
      ImmigrantInfo(CreatureId::DOPPLEGANGER, {MinionTrait::FIGHTER})
          .setFrequency(0.3)
          .setSpawnLocation(FurnitureType::DEMON_SHRINE)
          .addRequirement(AttractionInfo{3, FurnitureType::DEMON_SHRINE})
          .addPreliminaryRequirement(FurnitureType::DEMON_SHRINE),
      ImmigrantInfo(CreatureId::RAVEN, {MinionTrait::FIGHTER, MinionTrait::NO_RETURNING})
          .setFrequency(0.5)
          .addPreliminaryRequirement(SunlightState::DAY),
      ImmigrantInfo(CreatureId::BAT, {MinionTrait::FIGHTER, MinionTrait::NO_RETURNING})
          .setFrequency(0.5)
          .addPreliminaryRequirement(SunlightState::NIGHT),
      ImmigrantInfo(CreatureId::WOLF, {MinionTrait::FIGHTER, MinionTrait::NO_RETURNING})
          .setFrequency(0.15)
          .setGroupSize(Range(3, 9))
          .setAutoTeam()
          .addPreliminaryRequirement(SunlightState::NIGHT),
      ImmigrantInfo(CreatureId::CAVE_BEAR, {MinionTrait::FIGHTER, MinionTrait::NO_RETURNING})
          .setFrequency(0.1),
      ImmigrantInfo(CreatureId::WEREWOLF, {MinionTrait::FIGHTER, MinionTrait::NO_RETURNING})
          .setFrequency(0.1)
          .addRequirement(AttractionInfo{2, FurnitureType::TRAINING_IRON}),
      ImmigrantInfo(CreatureId::DARK_ELF_WARRIOR, {MinionTrait::FIGHTER})
          .addPreliminaryRequirement(RecruitmentInfo{EnemyId::DARK_ELVES, 3, MinionTrait::FIGHTER})
          .addRequirement(CostInfo(CollectiveResourceId::GOLD, 150)),
      ImmigrantInfo({CreatureId::SPECIAL_HMBN, CreatureId::SPECIAL_HMBW,
              CreatureId::SPECIAL_HMGN, CreatureId::SPECIAL_HMGW}, {MinionTrait::FIGHTER})
          .addPreliminaryRequirement(TechId::HUMANOID_MUT)
          .addPreliminaryRequirement(Pregnancy {})
          .addRequirement(CostInfo(CollectiveResourceId::MANA, 250))
          .setSpawnLocation(Pregnancy {}),
      ImmigrantInfo({CreatureId::SPECIAL_BMBN, CreatureId::SPECIAL_BMBW,
              CreatureId::SPECIAL_BMGN, CreatureId::SPECIAL_BMGW}, {MinionTrait::FIGHTER})
          .addPreliminaryRequirement(TechId::BEAST_MUT)
          .addPreliminaryRequirement(Pregnancy {})
          .addRequirement(CostInfo(CollectiveResourceId::MANA, 250))
          .setSpawnLocation(Pregnancy {})
  });
}

static map<CollectiveResourceId, int> getKeeperCredit(bool resourceBonus) {
  if (resourceBonus) {
    map<CollectiveResourceId, int> credit;
    for (auto elem : ENUM_ALL(CollectiveResourceId))
      credit[elem] = 10000;
    return credit;
  } else
    return {{CollectiveResourceId::MANA, 200}};

}

static EnumSet<MinionTrait> getImpTraits() {
  return {MinionTrait::WORKER, MinionTrait::NO_LIMIT, MinionTrait::NO_EQUIPMENT};
}

PModel ModelBuilder::tryQuickModel(int width) {
  Model* m = new Model();
  string keeperName = options->getStringValue(OptionId::KEEPER_NAME);
  Level* top = m->buildTopLevel(
      LevelBuilder(meter, random, width, width, "Quick", false),
      LevelMaker::quickLevel(random));
  m->calculateStairNavigation();
  m->collectives.push_back(CollectiveBuilder(
        getKeeperConfig(options->getBoolValue(OptionId::FAST_IMMIGRATION)), TribeId::getKeeper())
      .setLevel(top)
      .setCredit(getKeeperCredit(true))
      .build());
  vector<CreatureId> ids {
    CreatureId::DONKEY,
  };
  for (auto elem : ids) {
    PCreature c = CreatureFactory::fromId(elem, TribeId::getKeeper(),
        MonsterAIFactory::monster());
    top->landCreature(StairKey::keeperSpawn(), c.get());
//    m->playerCollective->addCreature(c.get(), {MinionTrait::FIGHTER});
    m->addCreature(std::move(c));
  }
  return PModel(m);
}

PModel ModelBuilder::quickModel() {
  return tryBuilding(5000, [=] { return tryQuickModel(40); });
}

SettlementInfo& ModelBuilder::makeExtraLevel(Model* model, EnemyInfo& enemy) {
  const int towerHeight = random.get(7, 12);
  const int gnomeHeight = random.get(3, 5);
  SettlementInfo& mainSettlement = enemy.settlement;
  SettlementInfo& extraSettlement = enemy.levelConnection->otherEnemy->settlement;
  switch (enemy.levelConnection->type) {
    case LevelConnection::TOWER: {
      StairKey downLink = StairKey::getNew();
      extraSettlement.upStairs = {downLink};
      for (int i : Range(towerHeight - 1)) {
        StairKey upLink = StairKey::getNew();
        model->buildLevel(
            LevelBuilder(meter, random, 4, 4, "Tower floor" + toString(i + 2)),
            LevelMaker::towerLevel(random,
                CONSTRUCT(SettlementInfo,
                  c.type = SettlementType::TOWER;
                  c.creatures = CreatureFactory::singleType(TribeId::getHuman(), random.choose(
                      CreatureId::WATER_ELEMENTAL, CreatureId::AIR_ELEMENTAL, CreatureId::FIRE_ELEMENTAL,
                      CreatureId::EARTH_ELEMENTAL));
                  c.numCreatures = random.get(1, 3);
                  c.location = new Location();
                  c.upStairs = {upLink};
                  c.downStairs = {downLink};
                  c.furniture = FurnitureFactory(TribeId::getHuman(), FurnitureType::TORCH);
                  c.buildingId = BuildingId::BRICK;)));
        downLink = upLink;
      }
      mainSettlement.downStairs = {downLink};
      model->buildLevel(
         LevelBuilder(meter, random, 5, 5, "Tower top"),
         LevelMaker::towerLevel(random, mainSettlement));
      return extraSettlement;
    }
    case LevelConnection::CRYPT: {
      StairKey key = StairKey::getNew();
      extraSettlement.downStairs = {key};
      mainSettlement.upStairs = {key};
      model->buildLevel(
         LevelBuilder(meter, random, 40, 40, "Crypt"),
         LevelMaker::cryptLevel(random, mainSettlement));
      return extraSettlement;
    }
    case LevelConnection::MAZE: {
      StairKey key = StairKey::getNew();
      extraSettlement.upStairs = {key};
      mainSettlement.downStairs = {key};
      model->buildLevel(
         LevelBuilder(meter, random, 40, 40, "Maze"),
         LevelMaker::mazeLevel(random, extraSettlement));
      return mainSettlement;
    }
    case LevelConnection::GNOMISH_MINES: {
      StairKey upLink = StairKey::getNew();
      extraSettlement.downStairs = {upLink};
      for (int i : Range(gnomeHeight - 1)) {
        StairKey downLink = StairKey::getNew();
        model->buildLevel(
            LevelBuilder(meter, random, 60, 40, "Mines lvl " + toString(i + 1)),
            LevelMaker::roomLevel(random, CreatureFactory::gnomishMines(
                mainSettlement.tribe, TribeId::getMonster(), 0),
                CreatureFactory::waterCreatures(mainSettlement.tribe),
                CreatureFactory::lavaCreatures(mainSettlement.tribe), {upLink}, {downLink},
                FurnitureFactory::roomFurniture(TribeId::getPest())));
        upLink = downLink;
      }
      mainSettlement.upStairs = {upLink};
      model->buildLevel(
         LevelBuilder(meter, random, 60, 40, "Mine Town"),
         LevelMaker::mineTownLevel(random, mainSettlement));
      return extraSettlement;
    }
    case LevelConnection::SOKOBAN:
      StairKey key = StairKey::getNew();
      extraSettlement.upStairs = {key};
      mainSettlement.downStairs = {key};
      for (int i : Range(5000)) {
        try {
          Table<char> sokoLevel = sokobanInput->getNext();
          model->buildLevel(
              LevelBuilder(meter, random, sokoLevel.getBounds().width(), sokoLevel.getBounds().height(), "Sokoban"),
              LevelMaker::sokobanFromFile(random, mainSettlement, sokoLevel));
          return extraSettlement;
        } catch (LevelGenException) {
          INFO << "Retrying";
        }
      }
      throw LevelGenException();
  }
}

static string getBoardText(const string& keeperName, const string& dukeName) {
  return dukeName + " will reward a daring hero 150 florens for slaying " + keeperName + " the Keeper.";
}

PModel ModelBuilder::singleMapModel(const string& worldName) {
  return tryBuilding(10, [&] { return trySingleMapModel(worldName);});
}

PModel ModelBuilder::trySingleMapModel(const string& worldName) {
  vector<EnemyInfo> enemies;
  for (int i : Range(random.get(5, 9)))
    enemies.push_back(enemyFactory->get(EnemyId::HUMAN_COTTAGE));
  for (int i : Range(random.get(1, 3)))
    enemies.push_back(enemyFactory->get(EnemyId::KOBOLD_CAVE));
  for (int i : Range(random.get(1, 3)))
    enemies.push_back(enemyFactory->get(EnemyId::BANDITS).setSurprise().setVillainType(VillainType::LESSER));
  enemies.push_back(enemyFactory->get(random.choose(EnemyId::GNOMES, EnemyId::DARK_ELVES)).setSurprise().setVillainType(VillainType::ALLY));
  append(enemies, enemyFactory->getVaults());
  if (random.roll(4))
    enemies.push_back(enemyFactory->get(EnemyId::ANTS_CLOSED).setSurprise().setVillainType(VillainType::LESSER));
  enemies.push_back(enemyFactory->get(EnemyId::KNIGHTS).setSurprise().setVillainType(VillainType::MAIN));
  enemies.push_back(enemyFactory->get(EnemyId::FRIENDLY_CAVE).setSurprise().setVillainType(VillainType::ALLY));
  for (auto& enemy : random.chooseN(3, {
        EnemyId::ELEMENTALIST,
        EnemyId::WARRIORS,
        EnemyId::ELVES,
        EnemyId::DWARVES,
        EnemyId::VILLAGE}))
    enemies.push_back(enemyFactory->get(enemy).setSurprise().setVillainType(VillainType::MAIN));
  for (auto& enemy : random.chooseN(3, {
        EnemyId::GREEN_DRAGON,
        EnemyId::SHELOB,
        EnemyId::HYDRA,
        EnemyId::RED_DRAGON,
        EnemyId::CYCLOPS,
        EnemyId::DRIADS,
        EnemyId::ENTS}))
    enemies.push_back(enemyFactory->get(enemy).setSurprise().setVillainType(VillainType::LESSER));
  for (auto& enemy : random.chooseN(1, {
        EnemyId::WITCH,
        EnemyId::CEMETERY}))
    enemies.push_back(enemyFactory->get(enemy));
  return tryModel(360, worldName, enemies, true, BiomeId::GRASSLAND, {});
}

void ModelBuilder::addMapVillains(vector<EnemyInfo>& enemyInfo, BiomeId biomeId) {
  switch (biomeId) {
    case BiomeId::GRASSLAND:
      for (int i : Range(random.get(3, 5)))
        enemyInfo.push_back(enemyFactory->get(EnemyId::HUMAN_COTTAGE));
      break;
    case BiomeId::MOUNTAIN:
      for (int i : Range(random.get(1, 4)))
        enemyInfo.push_back(enemyFactory->get(random.choose(EnemyId::DWARF_CAVE, EnemyId::KOBOLD_CAVE)));
      break;
    case BiomeId::FORREST:
      for (int i : Range(random.get(3, 5)))
        enemyInfo.push_back(enemyFactory->get(EnemyId::ELVEN_COTTAGE));
      break;
  }
}

PModel ModelBuilder::tryCampaignBaseModel(const string& siteName, bool addExternalEnemies) {
  vector<EnemyInfo> enemyInfo;
  BiomeId biome = BiomeId::MOUNTAIN;
  addMapVillains(enemyInfo, biome);
  vector<ExternalEnemy> externalEnemies;
  if (addExternalEnemies)
    externalEnemies = enemyFactory->getExternalEnemies();
  return tryModel(210, siteName, enemyInfo, true, biome, externalEnemies);
}

static optional<BiomeId> getBiome(EnemyId enemyId, RandomGen& random) {
  switch (enemyId) {
    case EnemyId::KNIGHTS:
    case EnemyId::WARRIORS:
    case EnemyId::ELEMENTALIST:
    case EnemyId::LIZARDMEN:
    case EnemyId::HYDRA:
    case EnemyId::VILLAGE:
    case EnemyId::ORC_VILLAGE: return BiomeId::GRASSLAND;
    case EnemyId::RED_DRAGON:
    case EnemyId::GREEN_DRAGON:
    case EnemyId::DWARVES:
    case EnemyId::DARK_ELVES:
    case EnemyId::FRIENDLY_CAVE:
    case EnemyId::SOKOBAN:
    case EnemyId::GNOMES:
    case EnemyId::CYCLOPS:
    case EnemyId::SHELOB:
    case EnemyId::ANTS_OPEN: return BiomeId::MOUNTAIN;
    case EnemyId::ELVES:
    case EnemyId::DRIADS:
    case EnemyId::ENTS: return BiomeId::FORREST;
    case EnemyId::BANDITS: return random.choose<BiomeId>();
    case EnemyId::CEMETERY: return random.choose(BiomeId::GRASSLAND, BiomeId::FORREST);
    default: return none;
  }
}

PModel ModelBuilder::tryCampaignSiteModel(const string& siteName, EnemyId enemyId, VillainType type) {
  vector<EnemyInfo> enemyInfo { enemyFactory->get(enemyId).setVillainType(type).setSurprise()};
  auto biomeId = getBiome(enemyId, random);
  CHECK(biomeId) << "Unimplemented enemy in campaign " << EnumInfo<EnemyId>::getString(enemyId);
  addMapVillains(enemyInfo, *biomeId);
  return tryModel(170, siteName, enemyInfo, false, *biomeId, {});
}

PModel ModelBuilder::tryBuilding(int numTries, function<PModel()> buildFun) {
  for (int i : Range(numTries)) {
    try {
      if (meter)
        meter->reset();
      return buildFun();
    } catch (LevelGenException) {
      INFO << "Retrying level gen";
    }
  }
  FATAL << "Couldn't generate a level";
  return nullptr;

}

PModel ModelBuilder::campaignBaseModel(const string& siteName, bool externalEnemies) {
  return tryBuilding(20, [=] { return tryCampaignBaseModel(siteName, externalEnemies); });
}

PModel ModelBuilder::campaignSiteModel(const string& siteName, EnemyId enemyId, VillainType type) {
  return tryBuilding(20, [&] { return tryCampaignSiteModel(siteName, enemyId, type); });
}

void ModelBuilder::measureSiteGen(int numTries) {
  std::cout << "Measuring single map" << std::endl;
  measureModelGen(numTries, [this] { trySingleMapModel("pok"); });
  //measureModelGen(numTries, [this] { tryCampaignBaseModel("pok"); });
//  for (EnemyId id : {EnemyId::SOKOBAN})
  for (EnemyId id : ENUM_ALL(EnemyId))
    if (!!getBiome(id, random)) {
      std::cout << "Measuring " << EnumInfo<EnemyId>::getString(id) << std::endl;
      measureModelGen(numTries, [&] { tryCampaignSiteModel("", id, VillainType::LESSER); });
    }
}

void ModelBuilder::measureModelGen(int numTries, function<void()> genFun) {
  int numSuccess = 0;
  int maxT = 0;
  int minT = 1000000;
  double sumT = 0;
  for (int i : Range(numTries)) {
#ifndef OSX // this triggers some compiler errors OSX, I don't need it there anyway.
    auto time = steady_clock::now();
#endif
    try {
      genFun();
      ++numSuccess;
      std::cout << ".";
      std::cout.flush();
    } catch (LevelGenException) {
      std::cout << "x";
      std::cout.flush();
    }
#ifndef OSX
    int millis = duration_cast<milliseconds>(steady_clock::now() - time).count();
    sumT += millis;
    maxT = max(maxT, millis);
    minT = min(minT, millis);
#endif
  }
  std::cout << std::endl << numSuccess << " / " << numTries << " gens successful.\nMinT: " <<
    minT << "\nMaxT: " << maxT << "\nAvgT: " << sumT / numSuccess << std::endl;
}

Collective* ModelBuilder::spawnKeeper(Model* m) {
  Level* level = m->getTopLevel();
  PCreature keeper = CreatureFactory::fromId(CreatureId::KEEPER, TribeId::getKeeper());
  string keeperName = options->getStringValue(OptionId::KEEPER_NAME);
  if (!keeperName.empty())
    keeper->getName().setFirst(keeperName);
  Creature* keeperRef = keeper.get();
  CHECK(level->landCreature(StairKey::keeperSpawn(), keeperRef)) << "Couldn't place keeper on level.";
  m->addCreature(std::move(keeper));
  m->collectives.push_back(CollectiveBuilder(
        getKeeperConfig(options->getBoolValue(OptionId::FAST_IMMIGRATION)), TribeId::getKeeper())
      .setLevel(level)
      .addCreature(keeperRef)
      .setCredit(getKeeperCredit(options->getBoolValue(OptionId::STARTING_RESOURCE)))
      .build());
  Collective* playerCollective = m->collectives.back().get();
  playerCollective->setControl(PCollectiveControl(new PlayerControl(playerCollective, level)));
  playerCollective->setVillainType(VillainType::PLAYER);
  /*for (auto& elem : playerCollective->getImmigration().getAvailable())
    for (int i : Range(elem.second.get().getInfo().getInitialRecruitment()))
      playerCollective->getImmigration().accept(elem.first);*/
  return playerCollective;
}

PModel ModelBuilder::tryModel(int width, const string& levelName, vector<EnemyInfo> enemyInfo, bool keeperSpawn,
    BiomeId biomeId, vector<ExternalEnemy> externalEnemies) {
  Model* model = new Model();
  vector<SettlementInfo> topLevelSettlements;
  vector<EnemyInfo> extraEnemies;
  for (auto& elem : enemyInfo) {
    elem.settlement.collective = new CollectiveBuilder(elem.config, elem.settlement.tribe);
    if (elem.levelConnection) {
      elem.levelConnection->otherEnemy->settlement.collective =
          new CollectiveBuilder(elem.levelConnection->otherEnemy->config,
                                elem.levelConnection->otherEnemy->settlement.tribe);
      topLevelSettlements.push_back(makeExtraLevel(model, elem));
      extraEnemies.push_back(*elem.levelConnection->otherEnemy);
    } else
      topLevelSettlements.push_back(elem.settlement);
  }
  append(enemyInfo, extraEnemies);
  Level* top = model->buildTopLevel(
      LevelBuilder(meter, random, width, width, levelName, false),
      LevelMaker::topLevel(random, CreatureFactory::forrest(TribeId::getWildlife()), topLevelSettlements, width,
        keeperSpawn, biomeId));
  model->calculateStairNavigation();
  for (auto& enemy : enemyInfo) {
    if (!enemy.settlement.collective->hasCreatures())
      continue;
    PVillageControl control;
    Location* location = enemy.settlement.location;
    if (auto name = location->getName())
      enemy.settlement.collective->setLocationName(*name);
    if (auto race = enemy.settlement.race)
      enemy.settlement.collective->setRaceName(*race);
    PCollective collective = enemy.settlement.collective->addSquares(location->getAllSquares()).build();
    control.reset(new VillageControl(collective.get(), enemy.villain));
    if (enemy.villainType)
      collective->setVillainType(*enemy.villainType);
    if (enemy.id)
      collective->setEnemyId(*enemy.id);
    collective->setControl(std::move(control));
    model->collectives.push_back(std::move(collective));
  }
  if (keeperSpawn) {
    auto collective = spawnKeeper(model);
    if (!externalEnemies.empty())
      model->addExternalEnemies(ExternalEnemies(random, externalEnemies, collective));
  }
  return PModel(model);
}

PModel ModelBuilder::splashModel(const string& splashPath) {
  Model* m = new Model();
  Level* l = m->buildTopLevel(
      LevelBuilder(meter, Random, Level::getSplashBounds().width(), Level::getSplashBounds().height(), "Splash",
        true, 1.0),
      LevelMaker::splashLevel(
          CreatureFactory::splashLeader(TribeId::getHuman()),
          CreatureFactory::splashHeroes(TribeId::getHuman()),
          CreatureFactory::splashMonsters(TribeId::getKeeper()),
          CreatureFactory::singleType(TribeId::getKeeper(), CreatureId::IMP), splashPath));
  m->topLevel = l;
  return PModel(m);
}

