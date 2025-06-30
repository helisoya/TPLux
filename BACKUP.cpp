#include "lux/kit.hpp"
#include "lux/define.cpp"
#include <string.h>
#include <vector>
#include <set>
#include <stdio.h>

using namespace std;
using namespace lux;

bool initializedUnits = false;

enum UnitState
{
  DO_NOTHING,
  HARVEST_RESOURCE,
  BRING_RESOURCE_BACK,
  BUILD_CITY
};

struct UnitAction
{
  UnitState state;
  Position targetPosition;

  UnitAction(Position targetPosition)
  {
    state = DO_NOTHING;
    this->targetPosition = targetPosition;
  }
};

Position findClosestCityExpansion(Position position, Player &player, GameMap &map)
{
  if (player.cities.size() > 0)
  {
    Position deltas[4] = {Position(0, 1), Position(1, 0), Position(-1, 0), Position(0, -1)};

    auto city_iter = player.cities.begin();
    auto &city = city_iter->second;

    float closestDist = 999999;
    Cell *closestCityTile;
    Position newPos;
    Cell *newCell;
    for (auto &citytile : city.citytiles)
    {
      for (int i = 0; i < 4; i++)
      {
        newPos = Position(citytile.pos.x + deltas[i].x, citytile.pos.y + deltas[i].y);
        newCell = map.getCell(newPos.x, newPos.y);
        if (newCell == nullptr || newCell->citytile != nullptr || newCell->hasResource())
          continue;

        float dist = newCell->pos.distanceTo(position);
        if (dist < closestDist)
        {
          closestCityTile = newCell;
          closestDist = dist;
        }
      }
    }
    if (closestCityTile != nullptr)
    {
      return closestCityTile->pos;
    }
  }
  return position;
}

Position findClosestCity(Position position, Player &player)
{
  if (player.cities.size() > 0)
  {
    auto city_iter = player.cities.begin();
    auto &city = city_iter->second;

    float closestDist = 999999;
    CityTile *closestCityTile;
    for (auto &citytile : city.citytiles)
    {
      float dist = citytile.pos.distanceTo(position);
      if (dist < closestDist)
      {
        closestCityTile = &citytile;
        closestDist = dist;
      }
    }
    if (closestCityTile != nullptr)
    {
      return closestCityTile->pos;
    }
  }
  return position;
}

Position findClosestResource(Position position, Player &player, vector<Cell *> &resourceTiles)
{
  Cell *closestResourceTile;
  float closestDist = 9999999;
  for (auto it = resourceTiles.begin(); it != resourceTiles.end(); it++)
  {
    auto cell = *it;
    if (cell->resource.type == ResourceType::coal && !player.researchedCoal())
      continue;
    if (cell->resource.type == ResourceType::uranium && !player.researchedUranium())
      continue;

    int mult = cell->resource.type == ResourceType::coal ? 2 : (cell->resource.type == ResourceType::uranium) ? 1
                                                                                                              : 3;
    float dist = cell->pos.distanceTo(position) * mult;
    if (dist < closestDist)
    {
      closestDist = dist;
      closestResourceTile = cell;
    }
  }
  if (closestResourceTile != nullptr)
  {
    return closestResourceTile->pos;
  }
  return position;
}

int main()
{
  kit::Agent gameState = kit::Agent();
  // initialize
  gameState.initialize();
  vector<vector<UnitAction>> allActions;

  while (true)
  {
    /** Do not edit! **/
    // wait for updates
    gameState.update();

    if (!initializedUnits)
    {
      initializedUnits = true;

      Player &startup = gameState.players[0];
      for (int player = 0; player < 2; player++)
      {
        allActions.push_back(vector<UnitAction>());
        startup = gameState.players[player];
        for (int i = 0; i < startup.units.size(); i++)
        {
          allActions[player].push_back(UnitAction(startup.units[i].pos));
        }
      }
    }

    vector<string> actions = vector<string>();

    /** AI Code Goes Below! **/

    Player &player = gameState.players[gameState.id];
    Player &opponent = gameState.players[(gameState.id + 1) % 2];

    bool isDay = gameState.turn % 40 <= 25;

    vector<UnitAction> &playerUnitActions = allActions[gameState.id];

    GameMap &gameMap = gameState.map;

    vector<Cell *> resourceTiles = vector<Cell *>();
    for (int y = 0; y < gameMap.height; y++)
    {
      for (int x = 0; x < gameMap.width; x++)
      {
        Cell *cell = gameMap.getCell(x, y);
        if (cell->hasResource())
        {
          resourceTiles.push_back(cell);
        }
      }
    }

    // we iterate over all our units and do something with them
    for (int i = 0; i < player.units.size(); i++)
    {
      Unit unit = player.units[i];
      UnitAction &unitAction = playerUnitActions[i];
      if (unit.isWorker() && unit.canAct())
      {

        if (unitAction.state == HARVEST_RESOURCE)
        {
          if (unit.getCargoSpaceLeft() <= isDay ? 0 : 25)
          {
            Position newPos = findClosestCity(unit.pos, player);
            Cell *cell = gameMap.getCell(newPos.x, newPos.y);
            if (isDay && cell->citytile != nullptr && player.cities[cell->citytile->cityid].fuel > player.cities[cell->citytile->cityid].lightUpkeep * 10)
            {
              // Expand city
              newPos = findClosestCityExpansion(unit.pos, player, gameMap);
              actions.push_back(Annotate::text(newPos.x, newPos.y, "Expand City"));
              unitAction.targetPosition = newPos;
              unitAction.state = BUILD_CITY;
            }
            else
            {
              // Bring resources back
              unitAction.targetPosition = newPos;
              actions.push_back(Annotate::text(newPos.x, newPos.y, "Bring Resource Back"));
              unitAction.state = BRING_RESOURCE_BACK;
            }
          }
          else
          {
            // Check if target resource still exists
            Cell *cell = gameMap.getCell(unitAction.targetPosition.x, unitAction.targetPosition.y);
            if (!cell->hasResource())
            {
              Position newPos = findClosestResource(unit.pos, player, resourceTiles);
              unitAction.targetPosition = newPos;
              actions.push_back(Annotate::text(newPos.x, newPos.y, "Collect Resource"));
            }
          }
        }
        else if (unitAction.state == BRING_RESOURCE_BACK)
        {
          Cell *cell = gameMap.getCell(unitAction.targetPosition.x, unitAction.targetPosition.y);
          CityTile *citytile = cell->citytile;
          if (citytile != nullptr && unit.getCargoSpaceLeft() > 0)
          {
            // Go Harvest
            Position newPos = findClosestResource(unit.pos, player, resourceTiles);
            actions.push_back(Annotate::text(newPos.x, newPos.y, "Collect Resource"));
            unitAction.targetPosition = newPos;
            unitAction.state = HARVEST_RESOURCE;
          }
        }
        else if (unitAction.state == BUILD_CITY)
        {
          if (unit.pos.distanceTo(unitAction.targetPosition) == 0 && unit.canBuild(gameMap))
          {
            actions.push_back(unit.buildCity());
            unitAction.state = DO_NOTHING;
            continue;
          }
        }

        if (unit.pos.distanceTo(unitAction.targetPosition) > 0)
        {

          if (unitAction.state == BUILD_CITY)
          {
            Position dirDiff = Position(unitAction.targetPosition.x - unit.pos.x, unitAction.targetPosition.y - unit.pos.y);
            int xDiff = dirDiff.x;
            int yDiff = dirDiff.y;

            int ySign = yDiff < 0 ? -1 : (yDiff > 0 ? 1 : 0);
            int xSign = xDiff < 0 ? -1 : (xDiff > 0 ? 1 : 0);

            if (abs(yDiff) > abs(xDiff))
            {
              // if the move is greater in the y axis, then lets consider moving once in that dir

              Cell *checkTile = gameMap.getCell(unit.pos.x, unit.pos.y + ySign);
              if (checkTile->citytile == nullptr)
              {
                if (ySign == 1)
                  actions.push_back(unit.move(SOUTH));
                else
                  actions.push_back(unit.move(NORTH));
              }
              else
              {
                // there's a city tile, so we want to move in the other direction that we overall want to move
                if (xSign == 1)
                  actions.push_back(unit.move(EAST));
                else
                  actions.push_back(unit.move(WEST));
              }
            }
            else
            {
              // if the move is greater in the y axis, then lets consider moving once in that dir

              Cell *checkTile = gameMap.getCell(unit.pos.x + xSign, unit.pos.y);
              if (checkTile->citytile == nullptr)
              {
                if (xSign == 1)
                  actions.push_back(unit.move(EAST));
                else
                  actions.push_back(unit.move(WEST));
              }
              else
              {
                // there's a city tile, so we want to move in the other direction that we overall want to move
                if (yDiff == 1)
                  actions.push_back(unit.move(SOUTH));
                else
                  actions.push_back(unit.move(NORTH));
              }
            }
          }
          else
          {
            auto dir = unit.pos.directionTo(unitAction.targetPosition);
            actions.push_back(unit.move(dir));
          }
        }

        if (unitAction.state == DO_NOTHING)
        {
          Position newPos = findClosestResource(unit.pos, player, resourceTiles);
          actions.push_back(Annotate::text(newPos.x, newPos.y, "Collect Resource"));
          unitAction.targetPosition = newPos;
          unitAction.state = HARVEST_RESOURCE;
        }
      }
    }

    // Update cities
    if (player.cities.size() > 0)
    {
      auto city_iter = player.cities.begin();
      auto &city = city_iter->second;

      for (auto &citytile : city.citytiles)
      {
        if (citytile.canAct())
        {
          if (city.citytiles.size() > player.units.size())
          {
            actions.push_back(citytile.buildWorker());
            playerUnitActions.push_back(UnitAction(citytile.pos));
          }
          else
          {
            actions.push_back(citytile.research());
          }
        }
      }
    }

    // you can add debug annotations using the methods of the Annotate class.
    // actions.push_back(Annotate::circle(0, 0));

    /** AI Code Goes Above! **/

    /** Do not edit! **/
    for (int i = 0; i < actions.size(); i++)
    {
      if (i != 0)
        cout << ",";
      cout << actions[i];
    }
    cout << endl;
    // end turn
    gameState.end_turn();
  }

  return 0;
}
