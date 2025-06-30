#include "lux/kit.hpp"
#include "lux/define.cpp"
#include <string.h>
#include <vector>
#include <set>
#include <stdio.h>
#include <algorithm>
#include <iostream>
#include <queue>

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
  vector<Position> pathToTarget;
  int currentPathIdx;

  UnitAction(Position targetPosition)
  {
    state = DO_NOTHING;
    currentPathIdx = 0;
    this->targetPosition = targetPosition;
  }
};

struct Node
{
  int x, y;    // Coordinates of the node in the graph
  int f, g, h; // Values used by the A* algorithm

  Node(int _x, int _y) : x(_x), y(_y), f(0), g(0), h(0)
  {
  }

  // Overload comparison operators for priority queue
  bool operator>(const Node &other) const
  {
    return f > other.f;
  }

  bool operator==(const Node &other) const
  {
    return x == other.x && y == other.y;
  }
};

vector<Position> pathFindToTarget(Position start, Position end, GameMap &map, vector<Position> &units, int ignoreUnitIdx, Player &player, bool ignoreCities)
{
  // Define possible movements (4 directions: up, down, left, right)
  const int directionX[] = {-1, 0, 1, 0};
  const int directionY[] = {0, 1, 0, -1};

  // Initialize the open and closed lists
  priority_queue<Node, vector<Node>, greater<Node>> openList;
  vector<vector<bool>> closedList(map.height, vector<bool>(map.width, false));
  std::vector<std::vector<Node>> graph(map.height, vector<Node>(map.width, Node(0, 0)));

  Node startNode(start.x, start.y);
  Node endNode(end.x, end.y);

  // Start node
  openList.push(startNode);

  // Main loop
  while (!openList.empty())
  {
    // Get the cell with the lowest f value from the open list
    Node current = openList.top();
    openList.pop();

    // Check if the current cell is the goal
    if (current == endNode)
    {
      // Reconstruct the path
      vector<Position> path;
      while (!(current == startNode))
      {
        path.push_back(Position(current.x, current.y));
        current = graph[current.x][current.y];
      }
      path.push_back(start);
      reverse(path.begin(), path.end());
      return path;
    }

    // Mark the current cell as closed
    closedList[current.x][current.y] = true;

    // Explore neighbors
    for (int i = 0; i < 4; ++i)
    {
      int newX = current.x + directionX[i];
      int newY = current.y + directionY[i];

      // Check if the neighbor is within the grid boundaries
      if (newX >= 0 && newX < map.height && newY >= 0 && newY < map.width)
      {
        // Check if the neighbor is walkable and not in the closed list
        if (!closedList[newX][newY])
        {
          int price = 1;
          if (ignoreCities && player.cities.size() > 0)
          {
            auto city_iter = player.cities.begin();
            auto &city = city_iter->second;
            for (auto &citytile : city.citytiles)
            {
              if (citytile.pos.x == newX && citytile.pos.y == newY)
              {
                price = 999;
                break;
              }
            }
          }
          if (price == 1)
          {
            for (int idx = 0; idx < units.size(); idx++)
            {
              if (idx == ignoreUnitIdx)
                continue;
              if (units[idx].x == newX && units[idx].y == newY)
              {
                price = 999;
                break;
              }
            }
          }

          Node neighbor(newX, newY);
          int newG = current.g + price;

          // Check if the neighbor is not in the open list or has a lower g value
          if (newG < neighbor.g || !closedList[newX][newY])
          {
            neighbor.g = newG;
            neighbor.h = abs(newX - endNode.x) + abs(newY - endNode.y);
            neighbor.f = neighbor.g + neighbor.h;
            graph[newX][newY] = current; // Update the parent of the neighbor
            openList.push(neighbor);     // Add the neighbor to the open list
          }
        }
      }
    }
  }
  // No path found
  return vector<Position>();
}

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
  return Position(-1, -1);
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
  return Position(-1, -1);
}

Position findClosestResource(Position position, Player &player, vector<Cell *> &resourceTiles, std::string id, map<string, UnitAction> &unitActions)
{
  vector<Position> resourcesTaken;
  for (Unit &unit : player.units)
  {
    if (std::strcmp(unit.id.c_str(), id.c_str()) == 0)
      continue;
    if (unitActions[unit.id].state == HARVEST_RESOURCE)
      resourcesTaken.push_back(unitActions[unit.id].targetPosition);
  }

  Cell *closestResourceTile;
  float closestDist = 9999999;
  for (auto it = resourceTiles.begin(); it != resourceTiles.end(); it++)
  {
    auto cell = *it;
    if (cell->resource.type == ResourceType::coal && !player.researchedCoal())
      continue;
    if (cell->resource.type == ResourceType::uranium && !player.researchedUranium())
      continue;
    for (Position &pos : resourcesTaken)
    {
      if (pos == cell->pos)
        continue;
    }

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
  return Position(-1, -1);
}

bool startHarvestResource(Unit &unit, UnitAction &unitAction, GameMap &gameMap, vector<Position> unitsPositionsTemp, int unitIdx, vector<string> &actions, Player &player, vector<Cell *> &resourceTiles, map<string, UnitAction> &unitActions)
{
  Position selectedPosition = findClosestResource(unit.pos, player, resourceTiles, unit.id, unitActions);
  if (selectedPosition.x != -1 && selectedPosition.y != -1)
  {
    unitAction.state = HARVEST_RESOURCE;
    unitAction.targetPosition = selectedPosition;
    unitAction.pathToTarget = pathFindToTarget(unit.pos, selectedPosition, gameMap, unitsPositionsTemp, unitIdx, player, false);
    unitAction.currentPathIdx = 0;
    actions.push_back(Annotate::text(selectedPosition.x, selectedPosition.y, "Collect Resource"));
    std::cout << "Collect Resources : " << unitAction.pathToTarget.size() << std::endl;
    return true;
  }
  else
  {
    return false;
  }
}

bool startBringBackResource(Unit &unit, UnitAction &unitAction, GameMap &gameMap, vector<Position> unitsPositionsTemp, int unitIdx, vector<string> &actions, Player &player, vector<Cell *> &resourceTiles, map<string, UnitAction> &unitActions)
{
  Position selectedPosition = findClosestCity(unit.pos, player);
  if (selectedPosition.x != -1 && selectedPosition.y != -1)
  {
    unitAction.state = BRING_RESOURCE_BACK;
    unitAction.targetPosition = selectedPosition;
    unitAction.pathToTarget = pathFindToTarget(unit.pos, selectedPosition, gameMap, unitsPositionsTemp, unitIdx, player, false);
    unitAction.currentPathIdx = 0;
    actions.push_back(Annotate::text(selectedPosition.x, selectedPosition.y, "Bring back resources"));
    std::cout << "Bring back resources : " << unitAction.pathToTarget.size() << std::endl;
    return true;
  }
  else
  {
    return false;
  }
}

bool startExpandingCity(Unit &unit, UnitAction &unitAction, GameMap &gameMap, vector<Position> unitsPositionsTemp, int unitIdx, vector<string> &actions, Player &player, vector<Cell *> &resourceTiles, map<string, UnitAction> &unitActions)
{
  Position selectedPosition = findClosestCityExpansion(unit.pos, player, gameMap);
  if (selectedPosition.x != -1 && selectedPosition.y != -1)
  {
    unitAction.state = BUILD_CITY;
    unitAction.targetPosition = selectedPosition;
    unitAction.pathToTarget = pathFindToTarget(unit.pos, selectedPosition, gameMap, unitsPositionsTemp, unitIdx, player, false);
    unitAction.currentPathIdx = 0;
    actions.push_back(Annotate::text(selectedPosition.x, selectedPosition.y, "Build city"));
    std::cout << "Build city : " << unitAction.pathToTarget.size() << std::endl;
    return true;
  }
  else
  {
    return false;
  }
}

int main()
{
  kit::Agent gameState = kit::Agent();
  // initialize
  gameState.initialize();
  vector<map<string, UnitAction>> allActions;
  vector<Position> unitsPositionTemp;

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
        allActions.push_back(map<string, UnitAction>());
        startup = gameState.players[player];
        for (int i = 0; i < startup.units.size(); i++)
        {
          allActions[player][startup.units[i].id] = UnitAction(startup.units[i].pos);
        }
      }
    }

    vector<string> actions = vector<string>();

    /** AI Code Goes Below! **/

    Player &player = gameState.players[gameState.id];
    Player &opponent = gameState.players[(gameState.id + 1) % 2];

    bool isDay = gameState.turn % 40 <= 25;

    map<string, UnitAction> &playerUnitActions = allActions[gameState.id];

    unitsPositionTemp.clear();
    for (int i = 0; i < player.units.size(); i++)
    {
      unitsPositionTemp.push_back(player.units[i].pos);
    }

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

      if (playerUnitActions.find(unit.id) == playerUnitActions.end())
      {
        playerUnitActions[unit.id] = UnitAction(unit.pos);
      }

      UnitAction &unitAction = playerUnitActions[unit.id];

      if (unit.isWorker() && unit.canAct())
      {
        std::cout << "================" << std::endl;
        std::cout << "Unit " << i << std::endl;
        std::cout << unitAction.state << std::endl;

        if (unitAction.state == HARVEST_RESOURCE)
        {
          std::cout << "Harvest : " << (100 - unit.getCargoSpaceLeft()) << "/" << (100 - (isDay ? 0 : 25)) << std::endl;
          std::cout << "Harvest (Space Left) : " << unit.getCargoSpaceLeft() << " <= " << (isDay ? 0 : 25) << std::endl;
          if (unit.getCargoSpaceLeft() <= (isDay ? 0 : 25))
          {
            Position newPos = findClosestCity(unit.pos, player);
            Cell *cell = gameMap.getCell(newPos.x, newPos.y);
            if (isDay && cell->citytile != nullptr && player.cities[cell->citytile->cityid].fuel > player.cities[cell->citytile->cityid].lightUpkeep * 10)
            {
              // Expand city
              if (!startExpandingCity(unit, unitAction, gameMap, unitsPositionTemp, i, actions, player, resourceTiles, playerUnitActions))
              {
                if (!startBringBackResource(unit, unitAction, gameMap, unitsPositionTemp, i, actions, player, resourceTiles, playerUnitActions))
                {
                  unitAction.state = DO_NOTHING;
                }
              }
            }
            else
            {
              // Bring back resources
              if (!startBringBackResource(unit, unitAction, gameMap, unitsPositionTemp, i, actions, player, resourceTiles, playerUnitActions))
              {
                unitAction.state = DO_NOTHING;
              }
            }
          }
          else
          {
            // Check if target resource still exists
            Cell *cell = gameMap.getCell(unitAction.targetPosition.x, unitAction.targetPosition.y);
            if (!cell->hasResource())
            {
              if (!startHarvestResource(unit, unitAction, gameMap, unitsPositionTemp, i, actions, player, resourceTiles, playerUnitActions))
              {
                unitAction.state = DO_NOTHING;
              }
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

            if (!startBringBackResource(unit, unitAction, gameMap, unitsPositionTemp, i, actions, player, resourceTiles, playerUnitActions))
            {
              unitAction.state = DO_NOTHING;
            }
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
          else if (gameMap.getCellByPos(unitAction.targetPosition)->citytile != nullptr || unit.getCargoSpaceLeft() > 0)
          {
            unitAction.state = DO_NOTHING;
            continue;
          }
        }

        // Check if stuck
        if (unitAction.state != DO_NOTHING && unitAction.currentPathIdx < unitAction.pathToTarget.size() - 1)
        {
          bool locked = false;
          for (int otherUnits = 0; otherUnits < unitsPositionTemp.size(); otherUnits++)
          {
            if (otherUnits != i && (unitsPositionTemp[otherUnits] == unitAction.pathToTarget[unitAction.currentPathIdx + 1]))
            {
              locked = true;
              break;
            }
          }
          if (locked)
            continue;

          if (!locked && unitAction.state == BUILD_CITY && player.cities.size() > 0)
          {
            auto city_iter = player.cities.begin();
            auto &city = city_iter->second;
            for (auto &citytile : city.citytiles)
            {
              if (citytile.pos == unitAction.pathToTarget[unitAction.currentPathIdx + 1])
              {
                locked = true;
                break;
              }
            }
          }

          if (locked)
          {

            actions.push_back(Annotate::text(unit.pos.x, unit.pos.y, "Stuck, Recomputing..."));
            unitAction.pathToTarget = pathFindToTarget(unit.pos, unitAction.targetPosition, gameMap, unitsPositionTemp, i, player, unitAction.state == BUILD_CITY);
            unitAction.currentPathIdx = 0;
          }
        }

        if (unitAction.pathToTarget.size() == 0)
        {
          actions.push_back(Annotate::text(unit.pos.x, unit.pos.y, "No Pathfinding"));
        }
        else
        {
          std::cout << "Current Pathing : " << std::endl;
          std::cout << "Idx : " << unitAction.currentPathIdx << std::endl;
          for (int pathId = 0; pathId < unitAction.pathToTarget.size(); pathId++)
          {
            std::cout << unitAction.pathToTarget[pathId].x << " " << unitAction.pathToTarget[pathId].y << std::endl;
          }
        }

        std::cout << "Position : " << unit.pos.x << " " << unit.pos.y << std::endl;
        std::cout << "Target : " << unitAction.targetPosition.x << " " << unitAction.targetPosition.y << std::endl;

        if (unitAction.state != DO_NOTHING && unitAction.currentPathIdx < unitAction.pathToTarget.size() - 1)
        {
          if (unitAction.pathToTarget[unitAction.currentPathIdx + 1] == unit.pos)
          {
            unitAction.currentPathIdx++;
          }
          else
          {
            DIRECTIONS dir = unit.pos.directionTo(unitAction.pathToTarget[unitAction.currentPathIdx + 1]);
            if (dir != NULL && dir != CENTER)
            {
              unitAction.currentPathIdx++;
              std::cout << "Moving to : " << unitAction.pathToTarget[unitAction.currentPathIdx].x << " " << unitAction.pathToTarget[unitAction.currentPathIdx].y << std::endl;
              actions.push_back(unit.move(dir));
              unitsPositionTemp[i] = unitAction.pathToTarget[unitAction.currentPathIdx];
            }
          }
        }

        if (unitAction.state == DO_NOTHING)
        {
          if (!startBringBackResource(unit, unitAction, gameMap, unitsPositionTemp, i, actions, player, resourceTiles, playerUnitActions))
          {
            unitAction.state = DO_NOTHING;
          }
        }
      }
    }

    // Update cities
    if (player.cities.size() > 0)
    {
      int unitAmountOnTile;
      auto city_iter = player.cities.begin();
      auto &city = city_iter->second;

      for (auto &citytile : city.citytiles)
      {
        if (citytile.canAct())
        {
          unitAmountOnTile = 0;
          for (Unit &unit : player.units)
          {
            if (unit.pos == citytile.pos)
              unitAmountOnTile++;
          }

          if (city.citytiles.size() > player.units.size())
          {
            actions.push_back(citytile.buildWorker());
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