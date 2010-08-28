/**
 * class LootManager
 *
 * Handles floor loot
 *
 * @author Clint Bellanger
 * @license GPL
 */
 
#ifndef LOOT_MANAGER_H
#define LOOT_MANAGER_H

#include <string>
#include "SDL.h"
#include "SDL_image.h"
#include "SDL_mixer.h"

#include "Utils.h"
#include "ItemDatabase.h"
#include "MenuTooltip.h"
#include "EnemyManager.h"

struct LootDef {
	int item;
	int frame;
	Point pos;
};


// this means that normal items are 10x more common than epic items
// these numbers have to be balanced by various factors
const int RARITY_LOW = 7;
const int RARITY_NORMAL = 10;
const int RARITY_HIGH = 3;
const int RARITY_EPIC = 1;


class LootManager {
private:

	ItemDatabase *items;
	MenuTooltip *tip;
	EnemyManager *enemies;
	MapIso *map;

	// functions
	void loadGraphics();
	void calcTables();
	int lootLevel(int base_level);
	
	SDL_Surface *flying_loot[64];
	string animation_id[64];
	int animation_count;
	
	Mix_Chunk *loot_flip;
	
	Point frame_size;
	int frame_count; // the last frame is the "at-rest" floor loot graphic
	
	// loot refers to ItemDatabase indices
	LootDef loot[256]; // TODO: change to dynamic list without limits
	
	// loot tables multiplied out
	// currently loot can range from levels 0-20
	int loot_table[21][1024]; // level, number.  the int is an item id
	int loot_table_count[21]; // total number per level

public:
	LootManager(ItemDatabase *_items, MenuTooltip *_tip, EnemyManager *_enemies, MapIso *_map);
	~LootManager();

	void handleNewMap();
	void logic();
	void renderTooltips(Point cam);
	void checkEnemiesForLoot();
	void checkMapForLoot();
	void determineLoot(int base_level, Point pos);
	void addLoot(int item_id, Point pos);
	
	Renderable getRender(int index);
	
	int tooltip_margin;
	int loot_count;
	
};

#endif
