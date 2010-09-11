/**
 * class HazardManager
 *
 * Holds the collection of hazards (active attacks, spells, etc) and handles group operations
 *
 * @author Clint Bellanger
 * @license GPL
 */

#include "HazardManager.h"

HazardManager::HazardManager(PowerManager *_powers, Avatar *_hero, EnemyManager *_enemies) {
	powers = _powers;
	hero = _hero;
	enemies = _enemies;
	hazard_count = 0;
}

void HazardManager::logic() {
	// remove all hazards with lifespan 0
	for (int i=0; i<hazard_count; i++) {
		if (h[i]->lifespan == 0) {
			expire(i);
		}
	}
	
	checkNewHazards();
	
	// handle single-frame transforms
	for (int i=0; i<hazard_count; i++) {
		h[i]->logic();
	}
	
	bool hit;
	
	// handle collisions
	for (int i=0; i<hazard_count; i++) {
		if (h[i]->active && (h[i]->active_frame == -1 || h[i]->active_frame == h[i]->frame)) {
	
			// process hazards that can hurt enemies
			if (h[i]->source == SRC_HERO || h[i]->source == SRC_NEUTRAL) {
				for (int eindex = 0; eindex < enemies->enemy_count; eindex++) {
			
					// only check living enemies
					if (enemies->enemies[eindex]->stats.hp > 0 && h[i]->active) {
						if (isWithin(round(h[i]->pos), h[i]->radius, enemies->enemies[eindex]->stats.pos)) {
							// hit!
							hit = enemies->enemies[eindex]->takeHit(*h[i]);
							if (!h[i]->multitarget && hit) {
								h[i]->active = false;
								h[i]->lifespan = 0;
							}
						}
					}
				
				}
			}
		
			// process hazards that can hurt the hero
			if (h[i]->source == SRC_ENEMY || h[i]->source == SRC_NEUTRAL) {
				if (hero->stats.hp > 0 && h[i]->active) {
					if (isWithin(round(h[i]->pos), h[i]->radius, hero->stats.pos)) {
						// hit!
						hit = hero->takeHit(*h[i]);
						if (!h[i]->multitarget && hit) {
							h[i]->active = false;
							h[i]->lifespan = 0;
						}
					}
				}
			}
			
		}
	}
}

/**
 * Look for hazards generated this frame
 * TODO: all these hazards will originate from PowerManager instead
 */
void HazardManager::checkNewHazards() {

	Hazard *new_haz;

	// check PowerManager for hazards
	while (!powers->hazards.empty()) {
		new_haz = powers->hazards.front();		
		powers->hazards.pop();		
		new_haz->setCollision(collider);

		h[hazard_count] = new_haz;
		hazard_count++;
	}

	// check hero hazards
	if (hero->haz != NULL) {
		h[hazard_count] = hero->haz;
		hazard_count++;
		hero->haz = NULL;
	}
	
	// check monster hazards
	for (int eindex = 0; eindex < enemies->enemy_count; eindex++) {
		if (enemies->enemies[eindex]->haz != NULL) {
			h[hazard_count] = enemies->enemies[eindex]->haz;
			hazard_count++;
			enemies->enemies[eindex]->haz = NULL;
		}
	}
}

void HazardManager::expire(int index) {
	// TODO: assert this instead?
	if (index >= 0 && index < hazard_count) {
		delete(h[index]);
		for (int i=index; i<hazard_count-1; i++) {
			h[i] = h[i+1];
		}
		hazard_count--;
	}
}

/**
 * Reset all hazards and get new collision object
 */
void HazardManager::handleNewMap(MapCollision *_collider) {
	hazard_count = 0;
	collider = _collider;
}

/**
 * getRender()
 * Map objects need to be drawn in Z order, so we allow a parent object (GameEngine)
 * to collect all mobile sprites each frame.
 */
Renderable HazardManager::getRender(int haz_id) {

	Renderable r;
	r.map_pos.x = h[haz_id]->pos.x;
	r.map_pos.y = h[haz_id]->pos.y;
	r.sprite = h[haz_id]->sprites;
	r.src = new SDL_Rect();
	r.src->x = 64 * (h[haz_id]->frame / h[haz_id]->frame_duration);
	r.src->y = 64 * h[haz_id]->direction;
	r.src->w = 64;
	r.src->h = 64;
	r.offset.x = 32;
	r.offset.y = 64;
	r.object_layer = true;

	return r;
}

HazardManager::~HazardManager() {
	for (int i=0; i<hazard_count; i++) {
		delete(h[i]);
	}
}
