/**
 * class MapIso
 *
 * Isometric map data structure and rendering
 *
 * @author Clint Bellanger
 * @license GPL
 */
 
#include "MapIso.h"

MapIso::MapIso(SDL_Surface *_screen) {

	screen = _screen;

	// cam(x,y) is where on the map the camera is pointing
	// units found in Settings.h (UNITS_PER_TILE)
	cam.x = 0;
	cam.y = 0;
	
	new_music = false;
	
	clear_enemy(new_enemy);
	clearEvents();
	enemy_awaiting_queue = false;
	
	// spawn is a special map that defines where the campaign begins
	load("spawn.txt");
}


/**
 * trim: remove leading and trailing c from s
 */
string trim(string s, char c) {
	int first = 0;
	int last = s.length()-1;
	if (last <= 0) return "";

	while (s.at(first) == c && first < s.length()-1) {
		first++;
	}
	while (s.at(last) == c && last >= first) {
		last--;
	}
	if (first <= last) return s.substr(first,last-first+1);
	return "";
}

void MapIso::clearEvents() {
	for (int i=0; i<256; i++) {
		events[i].type = "";
		events[i].location.x = 0;
		events[i].location.y = 0;
		events[i].location.w = 0;
		events[i].location.h = 0;
		events[i].destination.x = 0;
		events[i].destination.y = 0;
		events[i].filename = "";
		events[i].value = 0;
	}
	event_count = 0;
}

string MapIso::parse_section_title(string s) {
	return s.substr(1, s.find_first_of(']') -1);
}

void MapIso::parse_key_pair(string s, string &key, string &val) {
	int colon = s.find_first_of('=');
	key = s.substr(0, colon);
	val = s.substr(colon+1, s.length());
}

/**
 * Given a string that starts with a number then a comma
 * Return that int, and modify the string to remove the num and comma
 *
 * This is basically a really lazy "split" replacement
 */
int MapIso::eatFirstInt(string &s, char separator) {
	int seppos = s.find_first_of(separator);
	int num = atoi(s.substr(0, seppos).c_str());
	s = s.substr(seppos+1, s.length());
	return num;
}

unsigned short MapIso::eatFirstHex(string &s, char separator) {
	int seppos = s.find_first_of(separator);
	unsigned short num = xtoi(s.substr(0, seppos));
	s = s.substr(seppos+1, s.length());
	return num;
}

void MapIso::clear_enemy(Map_Enemy e) {
	e.pos.x = 0;
	e.pos.y = 0;
	e.direction = 0;
	e.type = "";
}

/**
 * load
 */
int MapIso::load(string filename) {
	ifstream infile;
	string line;
	string starts_with;
	string section;
	string key;
	string val;
	string cur_layer;
	string data_format;
  
    event_count = 0;
  
	infile.open(("maps/" + filename).c_str(), ios::in);

	if (infile.is_open()) {
		while (!infile.eof()) {

			getline(infile, line);

			if (line.length() > 0) {
				starts_with = line.at(0);
				
				if (starts_with == "#") {
					// skip comments
				}
				else if (starts_with == "[") {
					section = trim(parse_section_title(line), ' ');
					
					data_format = "dec"; // default
					
					if (enemy_awaiting_queue) {
						enemies.push(new_enemy);
						enemy_awaiting_queue = false;
					}
					
					// for sections that are stored in collections, add a new object here
					if (section == "enemy") {
						clear_enemy(new_enemy);
						enemy_awaiting_queue = true;
					}
					else if (section == "event") {
						event_count++;
					}
					
				}
				else { // this is data.  treatment depends on section type
					parse_key_pair(line, key, val);          
					key = trim(key, ' ');
					val = trim(val, ' ');

					if (section == "header") {
						if (key == "title") {
							this->title = val;
						}
						else if (key == "width") {
							this->w = atoi(val.c_str());
						}
						else if (key == "height") {
							this->h = atoi(val.c_str());
						}
						else if (key == "tileset") {
							this->tileset = val;
						}
						else if (key == "music") {
							if (this->music_filename == val) {
								this->new_music = false;
							}
							else {
								this->music_filename = val;
								this->new_music = true;
							}
						}
						else if (key == "spawnpoint") {
							val = val + ",";
							spawn.x = eatFirstInt(val, ',') * UNITS_PER_TILE + UNITS_PER_TILE/2;
							spawn.y = eatFirstInt(val, ',') * UNITS_PER_TILE + UNITS_PER_TILE/2;
							spawn_dir = eatFirstInt(val, ',');
						}
					}
					else if (section == "layer") {
						if (key == "id") {
							cur_layer = val;
						}
						else if (key == "format") {
							data_format = val;
						}
						else if (key == "data") {
							// layer map data handled as a special case

							// The next h lines must contain layer data.  TODO: err
							if (data_format == "hex") {
								for (int j=0; j<h; j++) {
									getline(infile, line);
									line = line + ',';
									for (int i=0; i<w; i++) {
										if (cur_layer == "background") background[i][j] = this->eatFirstHex(line, ',');
										else if (cur_layer == "object") object[i][j] = this->eatFirstHex(line, ',');
										else if (cur_layer == "collision") collision[i][j] = this->eatFirstHex(line, ',');
									}
								}
							}
							else if (data_format == "dec") {
								for (int j=0; j<h; j++) {
									getline(infile, line);
									line = line + ',';
									for (int i=0; i<w; i++) {
										if (cur_layer == "background") background[i][j] = this->eatFirstInt(line, ',');
										else if (cur_layer == "object") object[i][j] = this->eatFirstInt(line, ',');
										else if (cur_layer == "collision") collision[i][j] = this->eatFirstInt(line, ',');
									}
								}
							}
						}
					}
					else if (section == "enemy") {
						
						if (key == "type") {
							new_enemy.type = val;
						}
						else if (key == "spawnpoint") {
							val = val + ",";
							new_enemy.pos.x = eatFirstInt(val, ',') * UNITS_PER_TILE + UNITS_PER_TILE/2;
							new_enemy.pos.y = eatFirstInt(val, ',') * UNITS_PER_TILE + UNITS_PER_TILE/2;
							new_enemy.direction = eatFirstInt(val, ',');
						}
					}
					else if (section == "event") {
						if (key == "type") {
							events[event_count-1].type = val;
						}
						else if (key == "location") {
							val = val + ",";
							events[event_count-1].location.x = eatFirstInt(val, ',');
							events[event_count-1].location.y = eatFirstInt(val, ',');
							events[event_count-1].location.w = eatFirstInt(val, ',');
							events[event_count-1].location.h = eatFirstInt(val, ',');							
						}
						else if (key == "destination") {
							val = val + ",";						
							events[event_count-1].destination.x = eatFirstInt(val, ',');
							events[event_count-1].destination.y = eatFirstInt(val, ',');							
						}
						else if (key == "filename") {
							events[event_count-1].filename = val;
						}
						else if (key == "value") {
							events[event_count-1].value = atoi(val.c_str());
						}
					}
				}
			}
		} // eof
		if (enemy_awaiting_queue) {
			enemies.push(new_enemy);
			enemy_awaiting_queue = false;
		}
	}

	infile.close();

	collider.setmap(collision);
	if (this->new_music) {
		loadMusic();
		this->new_music = false;
	}
	tset.load(this->tileset);

	return 0;
}

void MapIso::loadMusic() {

	if (music != NULL) {
		Mix_HaltMusic();
		Mix_FreeMusic(music);
		music = NULL;
	}
	music = Mix_LoadMUS(("music/" + this->music_filename).c_str());
	if (!music) {
	  printf("Mix_LoadMUS: %s\n", Mix_GetError());
	  SDL_Quit();
	}

	Mix_PlayMusic(music, -1);
	
}

void MapIso::render(Renderable r[], int rnum) {

	// r will become a list of renderables.  Everything not on the map already:
	// - hero
	// - npcs and monsters
	// - loot
	// maybe, special effects
	// we want to sort these by map draw order.  Then, we use a cursor to move through the 
	// renderables while we're also moving through the map tiles.  After we draw each map tile we
	// check to see if it's time to draw the next renderable yet.

	short unsigned int i;
	short unsigned int j;
	//SDL_Rect src;
	SDL_Rect dest;
	int current_tile;
	
	Point xcam;
	Point ycam;
	xcam.x = cam.x/UNITS_PER_PIXEL_X;
	xcam.y = cam.y/UNITS_PER_PIXEL_X;
	ycam.x = cam.x/UNITS_PER_PIXEL_Y;
	ycam.y = cam.y/UNITS_PER_PIXEL_Y;
	
	
	// todo: trim by screen rect
	// background
	for (j=0; j<h; j++) {
		for (i=0; i<w; i++) {
		  
			current_tile = background[i][j];
			
			if (current_tile > 0) {			
			
				dest.x = 320 + (i * TILE_W_HALF - xcam.x) - (j * TILE_W_HALF - xcam.y);
				dest.y = 240 + (i * TILE_H_HALF - ycam.x) + (j * TILE_H_HALF - ycam.y) + TILE_H_HALF;
				// adding TILE_H_HALF gets us to the tile center instead of top corner
				dest.x -= tset.tiles[current_tile].offset.x;
				dest.y -= tset.tiles[current_tile].offset.y;
				dest.w = tset.tiles[current_tile].src.w;
				dest.h = tset.tiles[current_tile].src.h;
				
				SDL_BlitSurface(tset.sprites, &(tset.tiles[current_tile].src), screen, &dest);
	
			}
		}
	}

	// todo: trim by screen rect
	// object layer
	for (j=0; j<h; j++) {
		for (i=0; i<w; i++) {
		
			current_tile = object[i][j];
			
			if (current_tile > 0) {			
			
				dest.x = 320 + (i * TILE_W_HALF - xcam.x) - (j * TILE_W_HALF - xcam.y);
				dest.y = 240 + (i * TILE_H_HALF - ycam.x) + (j * TILE_H_HALF - ycam.y) + TILE_H_HALF;
				// adding TILE_H_HALF gets us to the tile center instead of top corner
				dest.x -= tset.tiles[current_tile].offset.x;
				dest.y -= tset.tiles[current_tile].offset.y;
				dest.w = tset.tiles[current_tile].src.w;
				dest.h = tset.tiles[current_tile].src.h;
				
				SDL_BlitSurface(tset.sprites, &(tset.tiles[current_tile].src), screen, &dest);
	
			}
			
			// renderable entities go in this layer
			for (int ri = 0; ri < rnum; ri++) {			
				if (r[ri].map_pos.x / UNITS_PER_TILE == i && r[ri].map_pos.y / UNITS_PER_TILE == j) {
				
					// draw renderable
					dest.w = r[ri].src->w;
					dest.h = r[ri].src->h;
					dest.x = 320 + (r[ri].map_pos.x/UNITS_PER_PIXEL_X - xcam.x) - (r[ri].map_pos.y/UNITS_PER_PIXEL_X - xcam.y) - r[ri].offset.x;
					dest.y = 240 + (r[ri].map_pos.x/UNITS_PER_PIXEL_Y - ycam.x) + (r[ri].map_pos.y/UNITS_PER_PIXEL_Y - ycam.y) - r[ri].offset.y;

					SDL_BlitSurface(r[ri].sprite, r[ri].src, screen, &dest);
				} 
			}
		}
	}

		
}

void MapIso::checkEvents(Point loc) {
	Point maploc;
	maploc.x = loc.x >> TILE_SHIFT;
	maploc.y = loc.y >> TILE_SHIFT;
	for (int i=0; i<event_count; i++) {
		if (maploc.x >= events[i].location.x &&
		    maploc.y >= events[i].location.y &&
		    maploc.x <= events[i].location.x + events[i].location.w &&
			maploc.y <= events[i].location.y + events[i].location.h) {
			executeEvent(i);
		}
	}
}

void MapIso::executeEvent(int eid) {
	if (events[eid].type == "teleport") {
		teleportation = true;
		teleport_mapname = events[eid].filename;
		
		teleport_desination.x = events[eid].destination.x * UNITS_PER_TILE + UNITS_PER_TILE/2;
		teleport_desination.y = events[eid].destination.y * UNITS_PER_TILE + UNITS_PER_TILE/2;
	}
}

MapIso::~MapIso() {
	if (music != NULL) {
		Mix_HaltMusic();
		Mix_FreeMusic(music);
	}
}

