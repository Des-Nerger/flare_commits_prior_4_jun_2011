/**
 * class ItemDatabase
 *
 * @author Clint Bellanger
 * @license GPL
 */

#ifndef ITEM_DATABASE_H
#define ITEM_DATABASE_H

#include <string>
#include <sstream>
#include <fstream>

#include "SDL.h"
#include "SDL_image.h"
#include "SDL_mixer.h"
#include "UtilsParsing.h"
#include "StatBlock.h"
#include "MenuTooltip.h"

using namespace std;

const int ICON_SIZE_32 = 0;
const int ICON_SIZE_64 = 1;

const int ITEM_TYPE_OTHER = -1;
const int ITEM_TYPE_MAIN = 0;
const int ITEM_TYPE_BODY = 1;
const int ITEM_TYPE_OFF = 2;
const int ITEM_TYPE_ARTIFACT = 3;
const int ITEM_TYPE_CONSUMABLE = 4;
const int ITEM_TYPE_GEM = 5;

const int REQUIRES_PHYS = 0;
const int REQUIRES_MAG = 1;
const int REQUIRES_OFF = 2;
const int REQUIRES_DEF = 3;

const int SFX_NONE = -1;
const int SFX_BOOK = 0;
const int SFX_CLOTH = 1;
const int SFX_COINS = 2;
const int SFX_GEM = 3;
const int SFX_LEATHER = 4;
const int SFX_METAL = 5;
const int SFX_PAGE = 6;
const int SFX_MAILLE = 7;
const int SFX_OBJECT = 8;
const int SFX_HEAVY = 9;
const int SFX_WOOD = 10;
const int SFX_POTION = 11;

const int ITEM_QUALITY_LOW = 0;
const int ITEM_QUALITY_NORMAL = 1;
const int ITEM_QUALITY_HIGH = 2;
const int ITEM_QUALITY_EPIC = 3;

struct Item {
	string name;
	int level;
	int quality;
	int type;
	int icon32;
	int icon64;
	int dmg_min;
	int dmg_max;
	int abs_min;
	int abs_max;
	int req_stat;
	int req_val;
	string bonus_stat;
	int bonus_val;
	string effect;
	int sfx;
	string gfx;
	string loot;
};

class ItemDatabase {
private:
	SDL_Surface *screen;
	SDL_Surface *icons;
	SDL_Rect src;
	SDL_Rect dest;
	Mix_Chunk *sfx[12];

public:
	ItemDatabase(SDL_Surface *_screen, SDL_Surface *_icons);
	~ItemDatabase();
	void load();
	void loadSounds();
	void renderIcon(int item, int x, int y, int size);
	void playSound(int item);
	void playCoinsSound();	
	TooltipData getTooltip(int item, StatBlock *stats);
	TooltipData getShortTooltip(int item);
	void applyEquipment(StatBlock *stats, int equipped[4]);
	bool activate(int item, StatBlock *stats);

	Item items[1024];
};

#endif
