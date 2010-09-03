/**
 * class GameEngine
 *
 * Hands off the logic and rendering for the current game mode
 *
 * @author Clint Bellanger
 * @license GPL
 */

#include "GameEngine.h"

GameEngine::GameEngine(SDL_Surface *_screen, InputState *_inp) {
	screen = _screen;
	inp = _inp;
	done = false;

	font = new FontEngine();	
	map = new MapIso(_screen);
	pc = new Avatar(_inp, map);
	enemies = new EnemyManager(map);
	hazards = new HazardManager(pc, enemies);
	menu = new MenuManager(_screen, _inp, font, &pc->stats);
	loot = new LootManager(menu->items, menu->tip, enemies, map);
	
	menu->log->add("Welcome to OSARE v0.08.");
	menu->log->add("Use WASD or arrows to move.");
	
	cancel_lock = false;
	loadGame();
}


/**
 * Process all actions for a single frame
 * This includes some message passing between child object
 */
void GameEngine::logic() {

	int pickup;
	if (!inp->pressing[MAIN1]) inp->mouse_lock = false;
	
	// the game action is paused if any menus are opened
	if (!menu->pause) {
		
		// click to pick up loot on the ground
		if (inp->pressing[MAIN1] && !inp->mouse_lock && !menu->inv->full()) {
			int gold;
			
			pickup = loot->checkPickup(inp->mouse, map->cam, pc->pos, gold);
			if (pickup > 0) {
				inp->mouse_lock = true;
				menu->inv->add(pickup);
			}
			else if (gold > 0) {
				inp->mouse_lock = true;
				menu->inv->addGold(gold);
			}
		}
	
		pc->logic();
		
		enemies->heroPos = pc->pos;
		enemies->logic();
		hazards->logic();
		loot->logic();
		enemies->checkEnemiesforXP(&pc->stats);
		
	}
	
	// if the player has dropped an item from the inventory
	if (menu->drop_item > 0) {
		loot->addLoot(menu->drop_item, pc->pos);
		menu->drop_item = 0;
	}
	
	// check teleport
	if (map->teleportation) {
		map->teleportation = false;

		map->cam.x = pc->pos.x = map->teleport_destination.x;
		map->cam.y = pc->pos.y = map->teleport_destination.y;
		
		// process intermap teleport
		if (map->teleport_mapname != "") {
			map->load(map->teleport_mapname);
			enemies->handleNewMap();
			hazards->handleNewMap();
			loot->handleNewMap();
			
			// store this as the new respawn point
			map->respawn_map = map->teleport_mapname;
			map->respawn_point.x = pc->pos.x;
			map->respawn_point.y = pc->pos.y;
		}
	}
	
	// handle cancel key
	if (!inp->pressing[CANCEL]) cancel_lock = false;
	else if (inp->pressing[CANCEL] && !cancel_lock) {
		cancel_lock = true;
		if (menu->pause) {
			menu->closeAll();
		}
		else {
			saveGame();
			done = true;
		}
	}
	
	// if user closes the window
	if (inp->done) {
		saveGame();
		done = true;
	}
	
	// check for log messages from various child objects
	if (map->log_msg != "") {
		menu->log->add(map->log_msg);
		map->log_msg = "";
	}
	if (pc->log_msg != "") {
		menu->log->add(pc->log_msg);
		pc->log_msg = "";
	}
	
	menu->logic();
	
	// check change equipment
	if (menu->inv->changed_equipment) {
		pc->loadGraphics(menu->items->items[menu->inv->equipped[0]].gfx, 
		                 menu->items->items[menu->inv->equipped[1]].gfx, 
		                 menu->items->items[menu->inv->equipped[2]].gfx);
		menu->inv->changed_equipment = false;
	}
	
}

/**
 * Render all graphics for a single frame
 */
void GameEngine::render() {

	// Create a list of Renderables from all objects not already on the map.
	renderableCount = 0;

	r[renderableCount++] = pc->getRender(); // Avatar
	
	for (int i=0; i<enemies->enemy_count; i++) { // Enemies
		r[renderableCount++] = enemies->getRender(i);
	}
	
	for (int i=0; i<loot->loot_count; i++) { // Loot
		r[renderableCount++] = loot->getRender(i);
	}
	
	for (int i=0; i<hazards->hazard_count; i++) { // Hazards
		// TODO: hazard effects renderables
		// Not all hazards have a renderable component
	}
		
	zsort(r,renderableCount);

	// render the static map layers plus the renderables
	map->render(r, renderableCount);
	
	// display the name of the map in the upper-right hand corner
	font->render(map->title, VIEW_W-2, 2, JUSTIFY_RIGHT, screen, FONT_WHITE);
	
	// mouseover loot tooltips
	loot->renderTooltips(map->cam);
	
	menu->log->renderHUDMessages();
	menu->render();
}

GameEngine::~GameEngine() {
	delete(hazards);
	delete(enemies);
	delete(pc);
	delete(map);
	delete(font);
	delete(menu);
	delete(loot);
}
