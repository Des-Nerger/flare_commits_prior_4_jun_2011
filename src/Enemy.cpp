/*
 * class Enemy
 *
 * @author Clint Bellanger
 * @license GPL
 *
 */

#include "Enemy.h"

Enemy::Enemy(PowerManager *_powers, MapIso *_map) {
	powers = _powers;
	map = _map;

	stats.cur_state = ENEMY_STANCE;
	stats.cur_frame = 0;
	stats.disp_frame = 0;	
	stats.dir_ticks = FRAMES_PER_SEC;
	stats.patrol_ticks = 0;
	stats.cooldown = 0;
	stats.last_seen.x = -1;
	stats.last_seen.y = -1;
	stats.in_combat = false;
	
	haz = NULL;
	
	sfx_phys = false;
	sfx_ment = false;
	sfx_hit = false;
	sfx_die = false;
	sfx_critdie = false;
	loot_drop = false;
	reward_xp = false;
}

/**
 * move()
 * Apply speed to the direction faced.
 *
 * @return Returns false if wall collision, otherwise true.
 */
bool Enemy::move() {

	if (stats.immobilize_duration > 0) return false;

	int speed_diagonal = stats.dspeed;
	int speed_straight = stats.speed;
	
	if (stats.slow_duration > 0) {
		speed_diagonal /= 2;
		speed_straight /= 2;
	}
	else if (stats.haste_duration > 0) {
		speed_diagonal *= 2;
		speed_straight *= 2;
	}
	
	switch (stats.direction) {
		case 0:
			return map->collider.move(stats.pos.x, stats.pos.y, -1, 1, speed_diagonal);
		case 1:
			return map->collider.move(stats.pos.x, stats.pos.y, -1, 0, speed_straight);
		case 2:
			return map->collider.move(stats.pos.x, stats.pos.y, -1, -1, speed_diagonal);
		case 3:
			return map->collider.move(stats.pos.x, stats.pos.y, 0, -1, speed_straight);
		case 4:
			return map->collider.move(stats.pos.x, stats.pos.y, 1, -1, speed_diagonal);
		case 5:
			return map->collider.move(stats.pos.x, stats.pos.y, 1, 0, speed_straight);
		case 6:
			return map->collider.move(stats.pos.x, stats.pos.y, 1, 1, speed_diagonal);
		case 7:
			return map->collider.move(stats.pos.x, stats.pos.y, 0, 1, speed_straight);
	}
	return true;
}

/**
 * Change direction to face the target map location
 */
int Enemy::face(int mapx, int mapy) {

	// inverting Y to convert map coordinates to standard cartesian coordinates
	int dx = mapx - stats.pos.x;
	int dy = stats.pos.y - mapy;

	// avoid div by zero
	if (dx == 0) {
		if (dy > 0) return 3;
		else return 7;
	}
	
	float slope = ((float)dy)/((float)dx);
	if (0.5 <= slope && slope <= 2.0) {
		if (dy > 0) return 4;
		else return 0;
	}
	if (-0.5 <= slope && slope <= 0.5) {
		if (dx > 0) return 5;
		else return 1;
	}
	if (-2.0 <= slope && slope <= -0.5) {
		if (dx > 0) return 6;
		else return 2;
	}
	if (2 <= slope || -2 >= slope) {
		if (dy > 0) return 3;
		else return 7;
	}
	return stats.direction;
}

/**
 * The current direction leads to a wall.  Try the next best direction, if one is available.
 */
int Enemy::faceNextBest(int mapx, int mapy) {
	int dx = abs(mapx - stats.pos.x);
	int dy = abs(mapy - stats.pos.y);
	switch (stats.direction) {
		case 0:
			if (dy > dx) return 7;
			else return 1;
		case 1:
			if (mapy > stats.pos.y) return 0;
			else return 2;
		case 2:
			if (dx > dy) return 1;
			else return 3;
		case 3:
			if (mapx < stats.pos.x) return 2;
			else return 4;
		case 4:
			if (dy > dx) return 3;
			else return 5;
		case 5:
			if (mapy < stats.pos.y) return 4;
			else return 6;
		case 6:
			if (dx > dy) return 5;
			else return 7;
		case 7:
			if (mapx > stats.pos.x) return 6;
			else return 0;
	}
	return 0;
}

/**
 * Calculate distance between the enemy and the hero
 */
int Enemy::getDistance(Point dest) {
	int dx = dest.x - stats.pos.x;
	int dy = dest.y - stats.pos.y;
	double step1 = (double)dx * (double)dx + (double)dy * (double)dy;
	double step2 = sqrt(step1);
	return int(step2);
}

void Enemy::newState(int state) {
	
	stats.cur_state = state;
	stats.cur_frame = 0;
}
	
/**
 * logic()
 * Handle a single frame.  This includes:
 * - move the enemy based on AI % chances
 * - calculate the next frame of animation
 */
void Enemy::logic() {

	stats.logic();
	if (stats.stun_duration > 0) return;
	// check for bleeding to death
	if (stats.hp <= 0 && !(stats.cur_state == ENEMY_DEAD || stats.cur_state == ENEMY_CRITDEAD)) {
		doRewards();
		stats.cur_state = ENEMY_DEAD;
		stats.cur_frame = 0;
	}
	// check for bleeding spurt
	if (stats.bleed_duration % 30 == 1) {
		powers->activate(POWER_SPARK_BLOOD, &stats, stats.pos);
	}
	// check for teleport powers
	if (stats.teleportation) {
		stats.pos.x = stats.teleport_destination.x;
		stats.pos.y = stats.teleport_destination.y;	
		stats.teleportation = false;	
	}
	
	int dist;
	int prev_direction;
	bool los = false;
	Point pursue_pos;	
	int max_frame;
	int mid_frame;
	
	
	// SECTION 1: Steering and Vision
	// ------------------------------
	
	// check distance and line of sight between enemy and hero
	if (stats.hero_alive)
		dist = getDistance(stats.hero_pos);
	else
		dist = 0;
	
	// if the hero is too far away or dead, abandon combat and do nothing
	if (dist > stats.threat_range+stats.threat_range || !stats.hero_alive) {
		stats.in_combat = false;
		stats.patrol_ticks = 0;
		stats.last_seen.x = -1;
		stats.last_seen.y = -1;
	}

	if (dist < stats.threat_range && stats.hero_alive)
		los = map->collider.line_of_sight(stats.pos.x, stats.pos.y, stats.hero_pos.x, stats.hero_pos.y);
	else
		los = false;
		
	// if the enemy can see the hero, it pursues.
	// otherwise, it will head towards where it last saw the hero
	if (los && dist < stats.threat_range) {
		stats.in_combat = true;
		stats.last_seen.x = stats.hero_pos.x;
		stats.last_seen.y = stats.hero_pos.y;
	}
	else if (stats.last_seen.x >= 0 && stats.last_seen.y >= 0) {
		if (getDistance(stats.last_seen) <= (stats.speed+stats.speed) && stats.patrol_ticks == 0) {
			stats.last_seen.x = -1;
			stats.last_seen.y = -1;
			stats.patrol_ticks = 8; // start patrol; see note on "patrolling" below
		}		
	}
	

	
	// where is the creature heading?
	// TODO: add fleeing for X ticks
	if (los) {
		pursue_pos.x = stats.last_seen.x = stats.hero_pos.x;
		pursue_pos.y = stats.last_seen.y = stats.hero_pos.y;
		stats.patrol_ticks = 0;
	}
	else if (stats.in_combat) {
	
		// "patrolling" is a simple way to help steering.
		// When the enemy arrives at where he last saw the hero, it continues
		// walking a few steps.  This gives a better chance of re-establishing
		// line of sight around corners.
		
		if (stats.patrol_ticks > 0) {
			stats.patrol_ticks--;
			if (stats.patrol_ticks == 0) {
				stats.in_combat = false;
			}			
		}
		pursue_pos.x = stats.last_seen.x;
		pursue_pos.y = stats.last_seen.y;
	}


	
	// SECTION 2: States
	// -----------------
	
	switch(stats.cur_state) {
	
		case ENEMY_STANCE:
		
			// handle animation
		    stats.cur_frame++;
			
			// stance is a back/forth animation
			mid_frame = stats.anim_stance_frames * stats.anim_stance_duration;
			max_frame = mid_frame + mid_frame;
			if (stats.cur_frame >= max_frame) stats.cur_frame = 0;
			if (stats.cur_frame >= mid_frame)
				stats.disp_frame = (max_frame -1 - stats.cur_frame) / stats.anim_stance_duration + stats.anim_stance_position;
			else
				stats.disp_frame = stats.cur_frame / stats.anim_stance_duration + stats.anim_stance_position;
			
			if (stats.in_combat) {

				// update direction to face the target
				if (++stats.dir_ticks > stats.dir_favor && stats.patrol_ticks == 0) {
					stats.direction = face(pursue_pos.x, pursue_pos.y);				
					stats.dir_ticks = 0;
				}
		
				// performed ranged actions
				if (dist > stats.melee_range && stats.cooldown_ticks == 0) {

					// CHECK: ranged physical!
					//if (!powers->powers[stats.power_index[RANGED_PHYS]].requires_los || los) {
					if (los) {
						if ((rand() % 100) < stats.power_chance[RANGED_PHYS] && stats.power_ticks[RANGED_PHYS] == 0) {
							
							newState(ENEMY_RANGED_PHYS);
							break;
						}
					}
					// CHECK: ranged spell!
					//if (!powers->powers[stats.power_index[RANGED_MENT]].requires_los || los) {
					if (los) {			
						if ((rand() % 100) < stats.power_index[RANGED_MENT] && stats.power_ticks[RANGED_MENT] == 0) {
							
							newState(ENEMY_RANGED_MENT);
							break;
						}
					}
				
					// CHECK: flee!
					
					// CHECK: pursue!
					if ((rand() % 100) < stats.chance_pursue) {
						if (move()) { // no collision
							newState(ENEMY_MOVE);
						}
						else {
							// hit an obstacle, try the next best angle
							prev_direction = stats.direction;
							stats.direction = faceNextBest(pursue_pos.x, pursue_pos.y);
							if (move()) {
								newState(ENEMY_MOVE);
								break;
							}
							else stats.direction = prev_direction;
						}
					}
					
				}
				// perform melee actions
				else if (dist <= stats.melee_range && stats.cooldown_ticks == 0) {
				
					// CHECK: melee attack!
					//if (!powers->powers[stats.power_index[MELEE_PHYS]].requires_los || los) {
					if (los) {
						if ((rand() % 100) < stats.power_chance[MELEE_PHYS] && stats.power_ticks[MELEE_PHYS] == 0) {
							
							newState(ENEMY_MELEE_PHYS);
							break;
						}
					}
					// CHECK: melee ment!
					//if (!powers->powers[stats.power_index[MELEE_MENT]].requires_los || los) {
					if (los) {
						if ((rand() % 100) < stats.power_chance[MELEE_MENT] && stats.power_ticks[MELEE_MENT] == 0) {
													
							newState(ENEMY_MELEE_MENT);
							break;
						}
					}
				}
			}
			
			break;
		
		case ENEMY_MOVE:
		
			// handle animation
			stats.cur_frame++;
			
			// run is a looped animation
			max_frame = stats.anim_run_frames * stats.anim_run_duration;
			if (stats.cur_frame >= max_frame) stats.cur_frame = 0;
			stats.disp_frame = (stats.cur_frame / stats.anim_run_duration) + stats.anim_run_position;
			
			if (stats.in_combat) {

				if (++stats.dir_ticks > stats.dir_favor && stats.patrol_ticks == 0) {
					stats.direction = face(pursue_pos.x, pursue_pos.y);				
					stats.dir_ticks = 0;
				}
				
				if (dist > stats.melee_range && stats.cooldown_ticks == 0) {
				
					// check ranged physical!
					//if (!powers->powers[stats.power_index[RANGED_PHYS]].requires_los || los) {
					if (los) {
						if ((rand() % 100) < stats.power_chance[RANGED_PHYS] && stats.power_ticks[RANGED_PHYS] == 0) {
							
							newState(ENEMY_RANGED_PHYS);
							break;
						}
					}
					// check ranged spell!
					// if (!powers->powers[stats.power_index[RANGED_MENT]].requires_los || los) {
					if (los) {
						if ((rand() % 100) < stats.power_chance[RANGED_MENT] && stats.power_ticks[RANGED_MENT] == 0) {
							
							newState(ENEMY_RANGED_MENT);
							break;
						}
					}
				
					if (!move()) {
						// hit an obstacle.  Try the next best angle
						prev_direction = stats.direction;
						stats.direction = faceNextBest(pursue_pos.x, pursue_pos.y);
						if (!move()) {
							newState(ENEMY_STANCE);
							stats.direction = prev_direction;
						}
					}
				}
				else {
					newState(ENEMY_STANCE);
				}
			}
			else {
				newState(ENEMY_STANCE);
			}
			break;
			
		case ENEMY_MELEE_PHYS:
			
			// handle animation
			stats.cur_frame++;
			
			// melee is a play-once animation
			max_frame = stats.anim_melee_frames * stats.anim_melee_duration;
			stats.disp_frame = (stats.cur_frame / stats.anim_melee_duration) + stats.anim_melee_position;

			if (stats.cur_frame == 1) {
				sfx_phys = true;
			}

			// the attack hazard is alive for a single frame
			if (stats.cur_frame == max_frame/2 && haz == NULL) {
				powers->activate(stats.power_index[MELEE_PHYS], &stats, pursue_pos);
				stats.power_ticks[MELEE_PHYS] = stats.power_cooldown[MELEE_PHYS];
			}

			if (stats.cur_frame == max_frame-1) {
				newState(ENEMY_STANCE);
				stats.cooldown_ticks = stats.cooldown;
			}
			break;

		case ENEMY_RANGED_PHYS:
	
			// monsters turn to keep aim at the hero
			stats.direction = face(pursue_pos.x, pursue_pos.y);
			
			// handle animation
			stats.cur_frame++;
			max_frame = stats.anim_ranged_frames * stats.anim_ranged_duration;
			stats.disp_frame = (stats.cur_frame / stats.anim_ranged_duration) + stats.anim_ranged_position;

			if (stats.cur_frame == 1) {
				sfx_phys = true;
			}
			
			// the attack hazard is alive for a single frame
			if (stats.cur_frame == max_frame/2 && haz == NULL) {
				powers->activate(stats.power_index[RANGED_PHYS], &stats, pursue_pos);
				stats.power_ticks[RANGED_PHYS] = stats.power_cooldown[RANGED_PHYS];
			}
			
			if (stats.cur_frame == max_frame-1) {
				newState(ENEMY_STANCE);
				stats.cooldown_ticks = stats.cooldown;
			}
			break;

		
		case ENEMY_MELEE_MENT:
	
			// handle animation
			stats.cur_frame++;
			max_frame = stats.anim_ment_frames * stats.anim_ment_duration;
			stats.disp_frame = (stats.cur_frame / stats.anim_ment_duration) + stats.anim_ment_position;

			if (stats.cur_frame == 1) {
				sfx_ment = true;
			}
			
			// the attack hazard is alive for a single frame
			if (stats.cur_frame == max_frame/2 && haz == NULL) {
				powers->activate(stats.power_index[MELEE_MENT], &stats, pursue_pos);
				stats.power_ticks[MELEE_MENT] = stats.power_cooldown[MELEE_MENT];
			}
			
			if (stats.cur_frame == max_frame-1) {
				newState(ENEMY_STANCE);
				stats.cooldown_ticks = stats.cooldown;
			}
			break;

		case ENEMY_RANGED_MENT:
		
			// monsters turn to keep aim at the hero
			stats.direction = face(pursue_pos.x, pursue_pos.y);
	
			// handle animation
			stats.cur_frame++;
			max_frame = stats.anim_ment_frames * stats.anim_ment_duration;
			stats.disp_frame = (stats.cur_frame / stats.anim_ment_duration) + stats.anim_ment_position;

			if (stats.cur_frame == 1) {
				sfx_ment = true;
			}
			
			// the attack hazard is alive for a single frame
			if (stats.cur_frame == max_frame/2 && haz == NULL) {
				powers->activate(stats.power_index[RANGED_MENT], &stats, pursue_pos);
				stats.power_ticks[RANGED_MENT] = stats.power_cooldown[RANGED_MENT];
			}
			
			if (stats.cur_frame == max_frame-1) {
				newState(ENEMY_STANCE);
				stats.cooldown_ticks = stats.cooldown;
			}
			break;
	
		case ENEMY_HIT:
			// enemy has taken damage (but isn't dead)

			stats.cur_frame++;

			// hit is a back/forth animation
			mid_frame = stats.anim_hit_frames * stats.anim_hit_duration;
			max_frame = mid_frame + mid_frame;
			if (stats.cur_frame >= mid_frame)
				stats.disp_frame = (max_frame -1 - stats.cur_frame) / stats.anim_hit_duration + stats.anim_hit_position;
			else
				stats.disp_frame = stats.cur_frame / stats.anim_hit_duration + stats.anim_hit_position;
			

			
			if (stats.cur_frame == max_frame-1) {
				newState(ENEMY_STANCE);
			}
			
			break;
			
		case ENEMY_DEAD:
		
			// corpse means the creature is dead and done animating
			if (!stats.corpse) {
				max_frame = (stats.anim_die_frames-1) * stats.anim_die_duration;
				if (stats.cur_frame < max_frame) stats.cur_frame++;
				if (stats.cur_frame == max_frame) stats.corpse = true;
				stats.disp_frame = (stats.cur_frame / stats.anim_die_duration) + stats.anim_die_position;
				if (stats.cur_frame == 1) sfx_die = true;
			}

			break;
		
		case ENEMY_CRITDEAD:
			// critdead is an optional, more gruesome death animation
		
			// corpse means the creature is dead and done animating
			if (!stats.corpse) {
				max_frame = (stats.anim_critdie_frames-1) * stats.anim_critdie_duration;
				if (stats.cur_frame < max_frame) stats.cur_frame++;
				if (stats.cur_frame == max_frame) stats.corpse = true;
				stats.disp_frame = (stats.cur_frame / stats.anim_critdie_duration) + stats.anim_critdie_position;
				if (stats.cur_frame == 1) sfx_critdie = true;
			}
			
			break;
	}

}

/**
 * Whenever a hazard collides with an enemy, this function resolves the effect
 * Called by HazardManager
 *
 * Returns false on miss
 */
bool Enemy::takeHit(Hazard h) {
	if (stats.cur_state != ENEMY_DEAD && stats.cur_state != ENEMY_CRITDEAD) {
	
		if (!stats.in_combat) {
			stats.in_combat = true;
			stats.last_seen.x = stats.hero_pos.x;
			stats.last_seen.y = stats.hero_pos.y;
		}
	
		// auto-miss if recently attacked
		// this is mainly to prevent slow, wide missiles from getting multiple attack attempts
		if (stats.targeted > 0) return false;
		stats.targeted = 5;
		
		// if it's a miss, do nothing
	    if (rand() % 100 > (h.accuracy - stats.avoidance + 25)) return false; 
		
		// calculate base damage
		int dmg;
		if (h.dmg_max > h.dmg_min) dmg = rand() % (h.dmg_max - h.dmg_min + 1) + h.dmg_min;
		else dmg = h.dmg_min;

		// apply elemental resistance
		// TODO: make this generic
		if (h.trait_elemental == ELEMENT_FIRE) {
			dmg = (dmg * stats.attunement_fire) / 100;
		}
		if (h.trait_elemental == ELEMENT_WATER) {
			dmg = (dmg * stats.attunement_ice) / 100;			
		}
		
		// substract absorption from armor
		int absorption;
		if (!h.trait_armor_penetration) { // armor penetration ignores all absorption
			if (stats.absorb_min == stats.absorb_max) absorption = stats.absorb_min;
			else absorption = stats.absorb_min + (rand() % (stats.absorb_max - stats.absorb_min + 1));
			dmg = dmg - absorption;
			if (dmg < 1 && h.dmg_min >= 1) dmg = 1; // TODO: when blocking, dmg can be reduced to 0
			if (dmg < 0) dmg = 0;
		}

		// check for crits
		int true_crit_chance = h.crit_chance;
		if (stats.stun_duration > 0 || stats.immobilize_duration > 0 || stats.slow_duration > 0)
			true_crit_chance += h.trait_crits_impaired;
			
		bool crit = (rand() % 100) < true_crit_chance;
		if (crit) {
			dmg = dmg + h.dmg_max;
			map->shaky_cam_ticks = FRAMES_PER_SEC/2;
		}
		
		// apply damage
		stats.takeDamage(dmg);
		
		// damage always breaks stun
		if (dmg > 0) stats.stun_duration=0;
		
		// after effects
		if (stats.hp > 0) {
			if (h.stun_duration > stats.stun_duration) stats.stun_duration = h.stun_duration;
			if (h.slow_duration > stats.slow_duration) stats.slow_duration = h.slow_duration;
			if (h.bleed_duration > stats.bleed_duration) stats.bleed_duration = h.bleed_duration;
			if (h.immobilize_duration > stats.immobilize_duration) stats.immobilize_duration = h.immobilize_duration;
		}
		
		// post effect power
		Point pt;
		pt.x = pt.y = 0;
		if (h.post_power >= 0 && dmg > 0) {
			powers->activate(h.post_power, &stats, pt);
		}
		
		// interrupted to new state
		if (dmg > 0) {
			sfx_hit = true;
			stats.cur_frame = 0;
			
			if (stats.hp <= 0 && crit) {
				doRewards();
				stats.disp_frame = stats.anim_critdie_position;
				stats.cur_state = ENEMY_CRITDEAD;
			}
			else if (stats.hp <= 0) {
				doRewards();
				stats.disp_frame = stats.anim_die_position;
				stats.cur_state = ENEMY_DEAD;		
			}
			// don't go through a hit animation if stunned
			else if (h.stun_duration == 0) {
				stats.disp_frame = stats.anim_hit_position;
				stats.cur_state = ENEMY_HIT;
			}
		}
		
		return true;
	}
	return false;
}

/**
 * Upon enemy death, handle rewards (gold, xp, loot)
 */
void Enemy::doRewards() {

	int roll = rand() % 100;
	if (roll < stats.loot_chance) {
		loot_drop = true;
	}
	reward_xp = true;
	
	// some creatures create special loot if we're on a quest
	if (stats.quest_loot_requires != "") {
	
		// the loot manager will check quest_loot_id
		// if set (not zero), the loot manager will 100% generate that loot.
		if (map->camp->checkStatus(stats.quest_loot_requires) && !map->camp->checkStatus(stats.quest_loot_not)) {
			loot_drop = true;
		}
		else {
			stats.quest_loot_id = 0;
		}
	}
	
	// some creatures drop special loot the first time they are defeated
	// this must be done in conjunction with defeat status
	if (stats.first_defeat_loot > 0) {
		if (!map->camp->checkStatus(stats.defeat_status)) {
			loot_drop = true;
			stats.quest_loot_id = stats.first_defeat_loot;
		}
	}
	
	// defeating some creatures (e.g. bosses) affects the story
	if (stats.defeat_status != "") {
		map->camp->setStatus(stats.defeat_status);
	}

}

/**
 * getRender()
 * Map objects need to be drawn in Z order, so we allow a parent object (GameEngine)
 * to collect all mobile sprites each frame.
 */
Renderable Enemy::getRender() {
	Renderable r;
	r.map_pos.x = stats.pos.x;
	r.map_pos.y = stats.pos.y;
	r.src.x = stats.render_size.x * stats.disp_frame;
	r.src.y = stats.render_size.y * stats.direction;
	r.src.w = stats.render_size.x;
	r.src.h = stats.render_size.y;
	r.offset.x = stats.render_offset.x;
	r.offset.y = stats.render_offset.y;
	
	// draw corpses below objects so that floor loot is more visible
	r.object_layer = !stats.corpse;
	
	return r;	
}

Enemy::~Enemy() {
	delete haz;
}

