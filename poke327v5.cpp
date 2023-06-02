#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h> //this is for INT_MAX if I use it
#include <unistd.h>

#include <assert.h>
#include <stdbool.h>

#include <execinfo.h>
#include <stdio.h>
#include <ncurses.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cstring>
#include <climits>


// Adapted from https://stackoverflow.com/questions/9555837/how-to-know-caller-function-when-tracing-assertion-failure
void crash() {
  void* callstack[128];
  int i, frames = backtrace(callstack, 128);
  char** strs = backtrace_symbols(callstack, frames);
  for (i = 0; i < frames; ++i) {
    printw("%s\n", strs[i]);
  }
  free(strs);
  assert(false);
}

//CONSTANTS
const int MAX = 42069666;
const int NO_DIRECTION = -1, NORTH = 0, EAST = 1, SOUTH = 2, WEST = 3;
const int COLOR_CATWALL = 9;
#define KEY_ESC 27
int NUMTRAINERS = 0;

typedef enum CharacterType{
    CT_PLAYER,
    CT_HIKER,
    CT_RIVAL,
    CT_SWIMMER,
    CT_PACER,
    CT_WANDERER,
    CT_EXPLORER,
    CT_SENTRY,
    CT_OTHER,
} CharacterType;

typedef enum TerrainType{
    TT_NO_TERRAIN,
    TT_BOULDER,
    TT_TREE,
    TT_PATH,
    TT_BRIDGE,
    TT_PMART,
    TT_PCENTER,
    TT_TGRASS,
    TT_SGRASS,
    TT_MOUNTAIN,
    TT_FOREST,
    TT_WATER,
    TT_GATE,
    TT_GRAVE,
    TT_CATACOMB
} TerrainType;

//function that takes a characterType enum and returns the corresponding character
char CT_to_Char(int CT){
    switch(CT){
        case CT_PLAYER:   return '@';
        case CT_HIKER:    return 'H';
        case CT_RIVAL:    return 'R';
        case CT_SWIMMER:  return 'M';
        case CT_PACER:    return 'P';
        case CT_WANDERER: return 'W';
        case CT_EXPLORER: return 'E';
        case CT_SENTRY:   return 'S';
        default:          return '!'; //this means there was an error!
    }
}

/////////////////////////////////////////////////////////////PROTOTYPES///////////////////////////////////////////////
class Map;
class WorldMap;
class Character;
class PC;
class NPC;
class CatacombMap;

struct GQnode;
struct GodQueue;

int calc_Cost(int terrain, int chartype);
int findNextPos(NPC* character, Map* m, WorldMap* WM, PC* player);
int generateCharacters(Map* m, WorldMap* WM, PC* player, int numTrainers);
int enqueueAllChars(GodQueue* GQ, Map* m);

///////////////////////////////////////////////////////////N-Curses////////////////////////////////////////////////////////////////////////////

int initTerminal(){
    initscr();  //makes screen blank and sets it up for ncurses
    raw();      //takes uninterupted input
    noecho();   //doesn't show what you are typing
    curs_set(0);    //curs_set() is an ncursee function that sets the cursor visibility. 0 means invisible, 1 means visible, 2 means very visible
    keypad(stdscr, 1);   //allows you to use keys
    start_color();
    init_pair(COLOR_RED, COLOR_RED, COLOR_BLACK);
    init_pair(COLOR_GREEN, COLOR_GREEN, COLOR_BLACK);
    init_pair(COLOR_YELLOW, COLOR_YELLOW, COLOR_BLACK);
    init_pair(COLOR_BLUE, COLOR_BLUE, COLOR_BLACK);
    init_pair(COLOR_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(COLOR_CYAN, COLOR_CYAN, COLOR_BLACK);
    init_pair(COLOR_WHITE, COLOR_WHITE, COLOR_BLACK);
    init_pair(COLOR_CATWALL, COLOR_BLACK, COLOR_MAGENTA);
    return 0;
}

void clearScreen(){
    for (int i = 1; i<22; i++){
        for (int j = 0; j<80; j++){
            mvprintw(i,j, " ");
        }
    }
}

void clearScreen_top(){
    for (int i = 0; i<80; i++){
        mvprintw(0,i, " ");
    }
}

void createPanel(int topRow, int bottomRow, int leftCol, int rightCol){
    for (int i = topRow; i<= bottomRow; i++){
        for (int j = leftCol; j<=rightCol; j++){
            if(i == topRow || i == bottomRow || j == leftCol || j == rightCol){
                mvprintw(i,j,"*");
            }else{
                mvprintw(i,j, " ");
            }
        }
    }
}

void clearPanel(int topRow, int bottomRow, int leftCol, int rightCol){
    for (int i=topRow; i<=bottomRow; i++){
        for (int j=leftCol; j<=rightCol; j++){
            mvprintw(i,j," ");
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////GOD QUEUE//////////////////////////////////////////////////////
struct GQnode{
    int row;
    int col;
    int value;
    Character* character;
    struct GQnode* next;
};

struct GodQueue{
    GQnode* head;
    int length;
};

int GQinit(GodQueue* q){
    q->head = NULL;
    q->length = 0;

    return 0; //success
}

int GQenqueue(GodQueue* q, int newRow, int newCol, int newVal, Character* newCharacter){
    GQnode* newNode;
    if(!(newNode = (GQnode*)calloc(sizeof(*newNode), 1))){
        return 1; //means failed to make new node
    }
    newNode->row = newRow;
    newNode->col = newCol;
    newNode->value = newVal;
    newNode->character = newCharacter;

    if (q->length == 0){    //if queue is empty make new node the front
        q->head = newNode;
    }
    else if (q->head->value > newNode->value){
        newNode->next = q->head;
        q->head = newNode;
    }
    else{   //queue has nodes in it so put it in the correct place based on its value
        GQnode* temp = q->head;
        //assert(temp);
        while(temp->next != NULL){
            if (temp->next->value >= newNode->value) {
                break;
            }
            temp = temp->next;
        }
        newNode->next = temp->next;
        temp->next = newNode;
    }
    q->length++;
    return 0;
}

int GQdequeue(GodQueue* q, GQnode* returnNode){
    if(q->length <= 0){
        printw("\nERROR!!!!!: trying to call dequeue on an empty queue!\n");
        return 1; //nothing to dequeue
    }
    returnNode->row = q->head->row;
    returnNode->col = q->head->col;
    returnNode->value = q->head->value;
    returnNode->character = q->head->character;

    if (q->length == 1){   //there is a single item in the queue
        free(q->head);
        q->head = NULL; //you removed the last elements
    }
    else{   //there are multiple items in the queue
        GQnode* temp = q->head;
        q->head = q->head->next;  //change the front
        free(temp);
    }
    q->length--;
    return 0; //success
}

int GQdequeue_RC(GodQueue* q, int* retRow, int* retCol){
    
    if(q->length <= 0){
        printw("\nERROR: trying to call dequeue on an empty queue\n");
        return 1; //nothing to dequeue
    }
    *retRow = q->head->row;
    *retCol = q->head->col;

    if (q->length == 1){   //there is a single item in the queue
        free(q->head);
        q->head = NULL; //you removed the last elements
    }
    else{   //there are multiple items in the queue
        GQnode* temp = q->head;
        q->head = q->head->next;  //change the front
        free(temp);
    }
    q->length--;
    return 0; //success
}

int GQis_empty(GodQueue* q){
    return !q->length;
}

int GQsize(GodQueue* q){
    return q->length;
}

int GQdequeueAll(GodQueue* q){
    GQnode* temp = q->head;
    while(temp != NULL){
        temp = temp->next;
        free(q->head);
        q->length--;
        q->head = temp;
    }
    return 0;
}
//IDK IF THIS IS RIGHT
int GQdestroy(GodQueue* q){
    GQnode* tmp;
    while((tmp = q->head)){
        free(tmp);
        q->head = q->head->next;
    }

    q->length = 0;
    return 0;
}

// //This may not work due to ncurses
// int printGQ(GodQueue* q){
//     GQnode* current = q->head;
//     while((current != NULL)){
//         printw("(%d, %d): %d   ",current->row, current->col, current->value);
//         if (current->character != NULL){
//             printw("NPC: %d,   DIR: %d", current->character->type, current->character->direction);
//         }
//         printw("\n");
//         current = current->next;
//     }
//     printw("\n");
//     return 0;
// }

class WorldMap
{
public:
    Map* mapGrid[401][401];
    CatacombMap* catacombGrid[101][101];
    int hiker_CM[21][80];
    int rival_CM[21][80];
    GodQueue charQueue;

    PC* player;

    WorldMap(): mapGrid(), catacombGrid(){
        for(int i=0; i<401; i++){
            for(int j=0; j<401; j++){
                mapGrid[i][j] = NULL;
                if (i<101 && j<101){
                    catacombGrid[i][j] = NULL;
                }
            }
        }
        GQinit(&(charQueue));   //initialize the character queue
    }

    ~WorldMap(){
        for (int i = 0; i < 401; ++i) {
            for (int j = 0; j < 401; ++j) {
                Map* p = mapGrid[i][j];
                if (p != NULL) {
                    free(p);
                }
            }
        }
    }
};

/////////////////////////////////////////////////////////////////////POKEDEX/////////////////////////////////////////////////////////////////////////

class Pokemon{
public:
    int id;
    std::string identifier;
    int species_id; 
    int height;
    int weight;
    int base_experience;
    int order;
    int is_default;
};

class Moves{
public:
    int id;
    std::string identifier; 
    int generation_id;
    int type_id;
    int power;
    int pp;
    int accuracy;
    int priority;
    int target_id;
    int damage_class_id;
    int effect_id;
    int effect_chance;
    int contest_type_id;
    int contest_effect_id;
    int super_contest_effect_id;
};

class Pokemon_moves{
public:
    int pokemon_id;
    int version_group_id;
    int move_id;
    int pokemon_move_method_id;
    int level;
    int order;
};

class Pokemon_species{
public:
    int id;
    std::string identifier;
    int generation_id;
    int evolves_from_species_id;
    int evolution_chain_id;
    int color_id;
    int shape_id;
    int habitat_id;
    int gender_rate;
    int capture_rate;
    int base_happiness;
    int is_baby;
    int hatch_counter;
    int has_gender_differences;
    int growth_rate_id;
    int forms_switchable;
    int is_legendary;
    int is_mythical;
    int order;
    int conquest_order;
};

class Experience{
public:
    int growth_rate_id;
    int level;
    int experience;
};

class Type_names{
public:
    int type_id;
    int local_language_id;
    std::string name;
};

class Pokemon_stats{
public:
    int pokemon_id;
    int stat_id;
    int base_stat;
    int effort;
};

class Stats{
public:
    int id;
    int damage_class_id;
    std::string identifier;
    int is_battle_only;
    int game_index;
};

class Pokemon_types{
public:
    int pokemon_id;
    int type_id;
    int slot;
};


std::vector<std::string> parseLine(std::string curLine){
    std::vector<std::string> parsedInfo;
    std::stringstream curStream(curLine);
    std::string curData;

    while(getline(curStream, curData, ',')){
        if (curData.length() == 0){
            parsedInfo.push_back("42069666");
        }else{
            parsedInfo.push_back(curData);
        }
    }
    return parsedInfo;
}

std::ifstream openCSV(const char* filename){
    bool fileFound = false;
    std::string home = std::string(getenv("HOME")) + ".poke327/";
    std::vector<std::string> file_paths = {"/share/cs327/pokedex/pokedex/data/csv/", home + "pokedex/pokedex/data/csv/", "../Pokedex/"}; 
    std::ifstream file;
    for (auto path :  file_paths){
        file.open(path + filename);
        if (file.is_open()){
            fileFound = true;
            break;
        }
        else{
            file.clear();
        }
    }

    if (!fileFound){
        std::cout << "FILE NOT FOUND" << std::endl;
        throw "FILE NOT FOUND";
    }
    return file;
}

std::vector<Pokemon> parsePokemon(){
    std::ifstream file = openCSV("pokemon.csv");
    std::vector<Pokemon> returnVector;  //make a vector of pokemon objects
    std::string curLine;
    std::string parsedLine;
    getline(file, curLine); //ignore the first line of code
    //iterate through all lines of the file (starting at line 2)
    while(getline(file, curLine)){
        Pokemon curPokemon;
        std::vector<std::string> parsedInfo = parseLine(curLine);

        curPokemon.id = stoi(parsedInfo[0]);
        curPokemon.identifier = parsedInfo[1];
        curPokemon.species_id = stoi(parsedInfo[2]);
        curPokemon.height = stoi(parsedInfo[3]);
        curPokemon.weight = stoi(parsedInfo[4]);
        curPokemon.base_experience = stoi(parsedInfo[5]);
        curPokemon.order = stoi(parsedInfo[6]);
        curPokemon.is_default = stoi(parsedInfo[7]);

        returnVector.push_back(curPokemon);
    }
    return returnVector;
}

std::vector<Moves> parseMoves(){
    std::ifstream file = openCSV("moves.csv");
    std::vector<Moves> returnVector;  //make a vector of pokemon objects
    std::string curLine;
    std::string parsedLine;
    getline(file, curLine); //ignore the first line of code
    //iterate through all lines of the file (starting at line 2)
    while(getline(file, curLine)){
        Moves curMove;
        std::vector<std::string> parsedInfo = parseLine(curLine);

        curMove.id = stoi(parsedInfo[0]);
        curMove.identifier = parsedInfo[1];
        curMove.generation_id = stoi(parsedInfo[2]);
        curMove.type_id = stoi(parsedInfo[3]);
        curMove.power = stoi(parsedInfo[4]);
        if (curMove.power > 10000){ //some pokemon don't have power so to make the battles work properly I am setting this to 20
            curMove.power = 20;
        }
        curMove.pp = stoi(parsedInfo[5]);
        curMove.accuracy = stoi(parsedInfo[6]);
        curMove.priority = stoi(parsedInfo[7]);
        curMove.target_id = stoi(parsedInfo[8]);
        curMove.damage_class_id = stoi(parsedInfo[9]);
        curMove.effect_id = stoi(parsedInfo[10]);
        curMove.effect_chance = stoi(parsedInfo[11]);
        curMove.contest_type_id = stoi(parsedInfo[12]);
        curMove.contest_effect_id = stoi(parsedInfo[13]);
        
        //account for the last index being empty
        if (parsedInfo.size() < 15){
            curMove.super_contest_effect_id = MAX;
        }else{
            curMove.super_contest_effect_id = stoi(parsedInfo[14]);
        }

        returnVector.push_back(curMove);
    }
    return returnVector;
}

std::vector<Pokemon_moves> parsePokemonMoves(){
    std::ifstream file = openCSV("pokemon_moves.csv");
    std::vector<Pokemon_moves> returnVector;  //make a vector of pokemon objects
    std::string curLine;
    std::string parsedLine;
    getline(file, curLine); //ignore the first line of code
    //iterate through all lines of the file (starting at line 2)
    while(getline(file, curLine)){
        Pokemon_moves curPokeMove;
        std::vector<std::string> parsedInfo = parseLine(curLine);
        
        curPokeMove.pokemon_id = stoi(parsedInfo[0]);
        curPokeMove.version_group_id = stoi(parsedInfo[1]);
        curPokeMove.move_id = stoi(parsedInfo[2]);
        curPokeMove.pokemon_move_method_id = stoi(parsedInfo[3]);
        curPokeMove.level = stoi(parsedInfo[4]);
        //account for if the last element is empty
        if (parsedInfo.size() < 6){
            curPokeMove.order = MAX;
        }else{
            curPokeMove.order = stoi(parsedInfo[5]);
        }
        
        returnVector.push_back(curPokeMove);
    }
    return returnVector;
}

std::vector<Pokemon_species> parsePokemonSpecies(){
    std::ifstream file = openCSV("pokemon_species.csv");
    std::vector<Pokemon_species> returnVector;  //make a vector of pokemon objects
    std::string curLine;
    std::string parsedLine;
    getline(file, curLine); //ignore the first line of code
    //iterate through all lines of the file (starting at line 2)
    while(getline(file, curLine)){
        Pokemon_species curPokeSpecies;
        std::vector<std::string> parsedInfo = parseLine(curLine);

        curPokeSpecies.id = stoi(parsedInfo[0]);
        curPokeSpecies.identifier = parsedInfo[1];
        curPokeSpecies.generation_id = stoi(parsedInfo[2]);
        curPokeSpecies.evolves_from_species_id = stoi(parsedInfo[3]);
        curPokeSpecies.evolution_chain_id = stoi(parsedInfo[4]);
        curPokeSpecies.color_id = stoi(parsedInfo[5]);
        curPokeSpecies.shape_id = stoi(parsedInfo[6]);
        curPokeSpecies.habitat_id = stoi(parsedInfo[7]);
        curPokeSpecies.gender_rate = stoi(parsedInfo[8]);
        curPokeSpecies.capture_rate = stoi(parsedInfo[9]);
        curPokeSpecies.base_happiness = stoi(parsedInfo[10]);
        curPokeSpecies.is_baby = stoi(parsedInfo[11]);
        curPokeSpecies.hatch_counter = stoi(parsedInfo[12]);
        curPokeSpecies.has_gender_differences = stoi(parsedInfo[13]);
        curPokeSpecies.growth_rate_id = stoi(parsedInfo[14]);
        curPokeSpecies.forms_switchable = stoi(parsedInfo[15]);
        curPokeSpecies.is_legendary = stoi(parsedInfo[16]);
        curPokeSpecies.is_mythical = stoi(parsedInfo[17]);
        curPokeSpecies.order = stoi(parsedInfo[18]);
        //account for the last element being empty
        if (parsedInfo.size() < 20){
            curPokeSpecies.conquest_order = MAX;
        }else{
            curPokeSpecies.conquest_order = stoi(parsedInfo[19]);
        }
        
        returnVector.push_back(curPokeSpecies);
    }
    return returnVector;
}

std::vector<Experience> parseExperience(){
    std::ifstream file = openCSV("experience.csv");
    std::vector<Experience> returnVector;  //make a vector of pokemon objects
    std::string curLine;
    std::string parsedLine;
    getline(file, curLine); //ignore the first line of code
    //iterate through all lines of the file (starting at line 2)
    while(getline(file, curLine)){
        Experience curExperience;
        std::vector<std::string> parsedInfo = parseLine(curLine);

        curExperience.growth_rate_id = stoi(parsedInfo[0]);
        curExperience.level = stoi(parsedInfo[1]);
        curExperience.experience = stoi(parsedInfo[2]);
        
        returnVector.push_back(curExperience);
    }
    return returnVector;
}

std::vector<Type_names> parseTypeNames(){
    std::ifstream file = openCSV("type_names.csv");
    std::vector<Type_names> returnVector;  //make a vector of pokemon objects
    std::string curLine;
    std::string parsedLine;
    getline(file, curLine); //ignore the first line of code
    //iterate through all lines of the file (starting at line 2)
    while(getline(file, curLine)){
        Type_names curTypeNames;
        std::vector<std::string> parsedInfo = parseLine(curLine);

        curTypeNames.type_id = stoi(parsedInfo[0]);
        curTypeNames.local_language_id = stoi(parsedInfo[1]);
        curTypeNames.name = parsedInfo[2];
        
        returnVector.push_back(curTypeNames);       
    }
    return returnVector;
}

std::vector<Pokemon_stats> parsePokemonStats(){
    std::ifstream file = openCSV("pokemon_stats.csv");
    std::vector<Pokemon_stats> returnVector;  //make a vector of pokemon objects
    std::string curLine;
    std::string parsedLine;
    getline(file, curLine); //ignore the first line of code
    //iterate through all lines of the file (starting at line 2)
    while(getline(file, curLine)){
        Pokemon_stats curPokeStats;
        std::vector<std::string> parsedInfo = parseLine(curLine);

        curPokeStats.pokemon_id = stoi(parsedInfo[0]);
        curPokeStats.stat_id = stoi(parsedInfo[1]);
        curPokeStats.base_stat = stoi(parsedInfo[2]);
        curPokeStats.effort = stoi(parsedInfo[3]);
        
        returnVector.push_back(curPokeStats);
    }
    return returnVector;
}

std::vector<Stats> parseStats(){
    std::ifstream file = openCSV("stats.csv");
    std::vector<Stats> returnVector;  //make a vector of pokemon objects
    std::string curLine;
    std::string parsedLine;
    getline(file, curLine); //ignore the first line of code
    //iterate through all lines of the file (starting at line 2)
    while(getline(file, curLine)){
        Stats curStats;
        std::vector<std::string> parsedInfo = parseLine(curLine);

        curStats.id = stoi(parsedInfo[0]);
        curStats.damage_class_id = stoi(parsedInfo[1]);
        curStats.identifier = parsedInfo[2];
        curStats.is_battle_only = stoi(parsedInfo[3]);
        if (parsedInfo.size() < 5){
            curStats.game_index = MAX;
        }else{
            curStats.game_index = stoi(parsedInfo[4]);
        }
        
        returnVector.push_back(curStats);
    }
    return returnVector;
}

std::vector<Pokemon_types> parsePokemonTypes(){
    std::ifstream file = openCSV("pokemon_types.csv");
    std::vector<Pokemon_types> returnVector;  //make a vector of pokemon objects
    std::string curLine;
    std::string parsedLine;
    getline(file, curLine); //ignore the first line of code
    //iterate through all lines of the file (starting at line 2)
    while(getline(file, curLine)){
        Pokemon_types curPokeTypes;
        std::vector<std::string> parsedInfo = parseLine(curLine);

        curPokeTypes.pokemon_id = stoi(parsedInfo[0]);
        curPokeTypes.type_id = stoi(parsedInfo[1]);
        curPokeTypes.slot = stoi(parsedInfo[2]);
        
        returnVector.push_back(curPokeTypes);
    }
    return returnVector;
}


////////////////////////////////////////////////////////////////POKEMON GENERATION FUNCTIONS///////////////////////////////////////////////////////////////////

//Parse all the info from the CSV files into global vectors
std::vector<Pokemon> pokemonVector = parsePokemon();
std::vector<Moves> movesVector = parseMoves();
std::vector<Pokemon_moves> pokeMovesVector = parsePokemonMoves();
std::vector<Pokemon_species> pokeSpeciesVector = parsePokemonSpecies();
std::vector<Experience> experienceVector = parseExperience();
std::vector<Type_names> typeNamesVector = parseTypeNames();
std::vector<Pokemon_stats> pokeStatsVector = parsePokemonStats();
std::vector<Stats> statsVector = parseStats();
std::vector<Pokemon_types> pokeTypesVector = parsePokemonTypes();


//class that is the container for an inGamePokemon
class InGamePokemon{
public:
    std::string name;
    int pokemon_id;
    int pokemon_species_id;
    int base_experience;
    std::vector<Moves> moves;
    int level;
    int health;
    int curHealth;
    int baseHealth;
    int healthIV;
    int attack;
    int baseAttack;
    int attackIV;
    int defense;
    int baseDefense;
    int defenseIV;
    int special_attack;
    int baseSpecial_attack;
    int special_attackIV;
    int special_defense;
    int baseSpecial_defense;
    int special_defenseIV;
    int speed;
    int baseSpeed;
    int speedIV;
    int evasion;

    int gender; //0 is male, 1 is female
    bool isShiny;

    InGamePokemon(int wRow, int wCol){
        int randomPokeNum = rand()%1092;
        pokemon_id = pokemonVector[randomPokeNum].id; //generates a random pokemon
        pokemon_species_id = pokemonVector[randomPokeNum].species_id;
        base_experience = pokemonVector[randomPokeNum].base_experience; //sets the base experience of the pokemon
        name = pokemonVector[randomPokeNum].identifier; //set the name

        //GENERATE LEVEL
        int manDist = abs(wRow - 200) + abs(wCol - 200);
        if (manDist < 2){
            level = 1;
        }else if (manDist == 400){
            level = 100;
        }
        else if (manDist <=200){
            level = (rand()%(manDist/2))+1; //level is between 1 and DISTANCE/2 [integer division]
        }else{
            level = ((manDist-200)/2) + rand()%(100 - (manDist-200)/2); //level is between DISTANCE-200/2 and 100    [low + (rand()%(high-low))]
        }
        

        //GENERATE MOVES
        std::vector<Pokemon_moves> availableMoves;
        bool foundMoveGroup = false;
        int isNewMove = 1;
        while(1){ //this loop is broken once there are at least 2 available moves to select from
            for(long unsigned int i=0; i<pokeMovesVector.size(); i++){
                if (pokeMovesVector[i].pokemon_id == pokemon_species_id){ //check if the move's pokemon ID matches the pokemon you have
                    foundMoveGroup = true;  //this means you found where your pokemon's moves are grouped in the CSV file
                    if (pokeMovesVector[i].pokemon_move_method_id == 1 && pokeMovesVector[i].level <= level){ //the first condition was stated on the PDF and the second makes usre the pokemon has a high enough level to use the move
                        //MAKE SURE THE ID IS DIFFERENT FROM THE AVAILABLE ONES [this is due to duplicates because of the same move from different games being considered different]
                        for (long unsigned int k=0; k<availableMoves.size(); k++){
                            if (availableMoves[k].move_id == pokeMovesVector[i].move_id){ //if the move is already in the available moves array, don't add it again
                                isNewMove = 0;
                                break;
                            }
                        }
                        //only push back the move if it is truly new
                        if (isNewMove){
                            availableMoves.push_back(pokeMovesVector[i]);
                        }
                    }
                }
                if (pokeMovesVector[i].pokemon_id != pokemon_species_id && foundMoveGroup){ //if you passed the section of the CSV file where your pokemon's moves are, break out of the loop
                    break;
                }
            }
            if (availableMoves.size()>=1){
                break;
            }else{
                level++; //increase the pokemon's level and go through the moves again
                availableMoves.clear(); //clear out the moves array
            }
        }
        //choose random moves from the available moves
        int randMoveIndex;
        randMoveIndex = rand()%availableMoves.size();
        moves.push_back(movesVector[availableMoves[randMoveIndex].move_id-1]); //the -1 is because the movesVector is 0 indexed but the move_id is 1 indexed
        availableMoves.erase(availableMoves.begin() + randMoveIndex); //remove the element at position randMoveNum
        if (availableMoves.size() > 0){ //if you had more than 1 move to choose from, choose one more move
            randMoveIndex = rand()%availableMoves.size();
            moves.push_back(movesVector[availableMoves[randMoveIndex].move_id-1]);
        }

        //DETERMINE SHINYNESS [PDF says to use a 1/3192 chance but we can change it if we want]
        int randShinyNum = rand()%420;
        if (randShinyNum == 69){
            isShiny = true;
        }else{
            isShiny = false;
        }

        //ASSIGN GENDER (0=male, 1=female)
        gender = rand()%2;
    
        //GENERATE STATS
        long unsigned int curStatIndex=0; //your index in the pokemon_stats vector
        while(curStatIndex < pokeStatsVector.size()){
            if (pokeStatsVector[curStatIndex].pokemon_id == pokemon_id && pokeStatsVector[curStatIndex].stat_id == 1){ //checks if you are looking at the correct pokemon and correct stat [health is stat id 1]
                //Generate HP
                healthIV = rand()%16;
                baseHealth = pokeStatsVector[curStatIndex].base_stat;
                health = ((baseHealth + healthIV) * 2 * level)/100 + level + 10;  //HP = ((baseHP + IV) * 2 * lvl)/100 + lvl + 10
                curHealth = health; //set the current health to the max health
                
                //all subsequent stats should be next to each other and in the correct order in the CSV so we can just get the next few items of the CSV and they should match up with the stats listed in the stats CSV (in same order)
                
                //Generate Attack
                attackIV = rand()%16;
                baseAttack = pokeStatsVector[curStatIndex+1].base_stat;
                attack = ((baseAttack + attackIV) * 2 * level)/100 + 5; //STAT = ((baseSTAT + IV) * 2 * lvl)/100 + 5
                //generate Defense
                defenseIV = rand()%16;
                baseDefense = pokeStatsVector[curStatIndex+2].base_stat;
                defense = ((baseDefense + defenseIV) * 2 * level)/100 + 5;
                //generate Special Attack
                special_attackIV = rand()%16;
                baseSpecial_attack = pokeStatsVector[curStatIndex+3].base_stat;
                special_attack = ((baseSpecial_attack + special_attackIV) * 2 * level)/100 + 5;
                //generate Special Defense
                special_defenseIV = rand()%16;
                baseSpecial_defense = pokeStatsVector[curStatIndex+4].base_stat;
                special_defense = ((baseSpecial_defense+ special_defenseIV) * 2 * level)/100 + 5;
                //generate Speed
                speedIV = rand()%16;
                baseSpeed = pokeStatsVector[curStatIndex+5].base_stat;
                speed = ((baseSpeed + speedIV) * 2 * level)/100 + 5;
                break;
            }
            curStatIndex++;
        }
    }

    void levelUp(){
        level++;
        health = ((baseHealth + healthIV) * 2 * level)/100 + level + 10; 
        attack = ((baseAttack + attackIV) * 2 * level)/100 + 5;
        defense = ((baseDefense + defenseIV) * 2 * level)/100 + 5;
        special_attack = ((baseSpecial_attack + special_attackIV) * 2 * level)/100 + 5;
        special_defense = ((baseSpecial_defense + special_defenseIV) * 2 * level)/100 + 5;
        speed = ((baseSpeed + speedIV) * 2 * level)/100 + 5;
        return;
    }

};

//PROTOTYPE
int attack(InGamePokemon* attackerPoke, Moves* attackerMove, InGamePokemon* opponentPoke, std::string attacker);

//structure to store path direction probability
struct PathProb{
    int numV;       //stores how many vertical moves need to be made
    int numH;       //stores how many horizontal moves need to be made
    int dirV;       //tells if the V direction needs to move down (1) or up (-1) [signs were determined by how the grid indexing was layed out]
    int dirH;       //tells whether the Hshift needs to be right (1) or left (-1)
};

//initializing the structure to store Path probability
int initPathProb(PathProb* p, int startRow, int startCol, int goalRow, int goalCol){
    p->numV = abs(goalRow - startRow);
    p->numH = abs(goalCol - startCol);
    
    //determining vertical displacement direction
    if(goalRow-startRow > 0){
        p->dirV = 1;
    } else {
        p->dirV = -1;
    }

    //determining horizontal displacement direction
    if(goalCol-startCol > 0){
        p->dirH = 1;
    } else {
        p->dirH = -1;
    }

    return 0; //success
}

class Character{
public:
    int type;
    int row;
    int col;
    int nextRow;
    int nextCol;
    int nextCost;
    std::vector<InGamePokemon> pokemon;

    int updateCoords(int newRow, int newCol){
        row = newRow;
        col = newCol;
        return 0;
    }
    
    int updateNextCoords(int newNextRow, int newNextCol){
        nextRow = newNextRow;
        nextCol = newNextCol;
        return 0;
    }

    //This function returns the number of live pokemon in a party
    int numLivePokemon(){
        int numLive = 0;
        for(int n=0; n < int(pokemon.size()); n++){
            if (pokemon[n].curHealth > 0){
                numLive++;
            }
        }
        return numLive;
    }

    virtual ~Character(){}
};

class Map{
public:
    int Tgrid[21][80];
    Character* charGrid[21][80];
    int globalRow;
    int globalCol;
    
    //stores the location of a catacomb entrance on this map. If there isn't one, put (-1,-1)
    int catacombRow;
    int catacombCol;

    //stores loctaion of the gates on this map
    int gateN;
    int gateE;
    int gateS;
    int gateW;

    //generates an empty map
    Map(WorldMap* WM, int row, int col){
        for(int i=0; i<21; i++){
            for(int j=0; j<80; j++){
                Tgrid[i][j] = TT_NO_TERRAIN; //assigning every part of the map grid as 0 
                charGrid[i][j] = NULL;       //making every cell of the character map origionally null
            }
        }
        globalRow = row;
        globalCol = col;
        //possibly check to see if THIS map is out of bounds and whether a map already exists here in the world map (should be checked in main though)

        //Generate North Gate
        if(row == 0){                                   //you are at the top of the world so don't put a gate
            gateN=-1;
        }else if(WM->mapGrid[row-1][col] != NULL){      //north map exists so take the map's southern gate column number
            gateN = WM->mapGrid[row-1][col]->gateS;
        }else{                                          //no north map. create random gate location
            gateN = (rand() % 78) + 1;               //generate a random number from 1 to 78 for which COL to put the north enterance
        }
        //Generate East Gate
        if(col == 400){                                 //you are at the far right of the world so don't put a gate
            gateE=-1;
        }else if(WM->mapGrid[row][col+1] != NULL){      //East map exists so take the map's west gate row number
            gateE = WM->mapGrid[row][col+1]->gateW;
        }else{                                          //no East map. create random gate location
            gateE = (rand() % 19) + 1;               //generate a random number from 1 to 19 for which row to put the east enterance
        }
        //Generate South Gate
        if(row == 400){                                 //you are at the far bottom of the world so don't put a gate
            gateS = -1;
        }else if(WM->mapGrid[row+1][col] != NULL){      //South map exists so take the map's northern gate column number
            gateS = WM->mapGrid[row+1][col]->gateN;
        }else{                                          //no South map. create random gate location
            gateS = (rand() % 78) + 1;               //generate a random number from 1 to 78 for which col to put the south enterance
        }
        //Generate West Gate
        if(col == 0){                                   //you are at the far left of the world so don't put a gate
            gateW=-1;
        }else if(WM->mapGrid[row][col-1] != NULL){      //West map exists so take the map's east gate row number
            gateW = WM->mapGrid[row][col-1]->gateE;
        }else{                                          //no West map. create random gate location
            gateW = (rand() % 19) + 1;               //generate a random number from 1 to 19 for which row to put the West enterance
        }

        int k=0;
        
        //making border of map
        for(k=0; k<80; k++){ Tgrid[0][k] = TT_BOULDER; }
        for(k=0; k<21; k++){ Tgrid[k][79] = TT_BOULDER; }
        for(k=0; k<80; k++){ Tgrid[20][k] = TT_BOULDER; }
        for(k=0; k<21; k++){ Tgrid[k][0] = TT_BOULDER; }
        generateBiomes();
        generatePaths();
        placeRandomTerrain();
        generateBuildings();

        //there is a 1 in 3 chance of a catacomb spawning on any world map whose column and row is a multiple of 4 (so that traversing 1 catacomb map = traversing 4 overworld maps)
        if (globalRow%4 == 0 && globalCol%4 == 0 && rand()%5 < 4){
            generateCatacombEntrance();
        }else{
            catacombRow = -1;
            catacombCol = -1;
        }  

    }

    int generateBiomes(){
        srand(time(NULL));
        GodQueue TT_WATERBQ, mountainBQ, TT_TGRASSBQ1, TT_TGRASSBQ2, TT_SGRASSBQ1, TT_SGRASSBQ2, TT_FORESTBQ;
        GQinit(&TT_WATERBQ); GQinit(&mountainBQ); GQinit(&TT_TGRASSBQ1); GQinit(&TT_TGRASSBQ2); GQinit(&TT_SGRASSBQ1); GQinit(&TT_SGRASSBQ2); GQinit(&TT_FORESTBQ);
        int biomesList[7] = {TT_WATER, TT_MOUNTAIN, TT_TGRASS, TT_TGRASS, TT_SGRASS, TT_SGRASS, TT_FOREST};
        GodQueue BQlist[7] = {TT_WATERBQ, mountainBQ, TT_TGRASSBQ1, TT_TGRASSBQ2, TT_SGRASSBQ1, TT_SGRASSBQ2, TT_FORESTBQ};           //7 is the number of biomes there are
        
        //initialize all biomesqueues to empty
        for (int i=0; i<7; i++){
            GQinit(&BQlist[i]);
        }

        //seed all of the biomes & place surrounding cells into correct queue if they are in bounds
        int seedRow;
        int seedCol;
        int precedence = 0;
        for(int i=0; i<7; i++){                          //for each biome, create a seed
            seedRow = rand() % 19 + 1;                      //generates a number from 1 to 19
            seedCol = rand() % 78 + 1;                      //generates a number from 1 to 78
            while(Tgrid[seedRow][seedCol] != TT_NO_TERRAIN){   //if there is already a seed where you want to place a seed, you have to find a new location
                seedRow = rand() % 19 + 1;
                seedCol = rand() % 78 + 1;
            }
            Tgrid[seedRow][seedCol] = biomesList[i];   //place the correct character at the seed spot in the board
            
            //have 4 if statements to see whether surrounding spaces are in bounds. if they are, add them to the correct queue. All have precedence 0 becuase they are the first to be entered (FiFo queue)
            if(seedRow-1 >= 0){ GQenqueue(&BQlist[i], seedRow-1, seedCol, 0, NULL); } //check if space above seed is in bounds
            if(seedRow+1 <=20){ GQenqueue(&BQlist[i], seedRow+1, seedCol, 1, NULL); } //checks if space below seed is in bounds
            if(seedCol-1 >= 0){ GQenqueue(&BQlist[i], seedRow, seedCol-1, 2, NULL); }    //checks if space to left of seed is in bounds
            if(seedCol+1 <=79){ GQenqueue(&BQlist[i], seedRow, seedCol+1, 3, NULL); }//checks if space to right of seed is in bounds
        }

        //Grow biomes from the seeds
        int curRow;
        int curCol;
        precedence = 4; //a number to make sure the BQ dequeues correctly (the number starts at 0 and increase by 1 every time something is enqueued so that ones enqueued sooner have higher precedence)

        while(!(GQis_empty(&BQlist[0]) && GQis_empty(&BQlist[1]) && GQis_empty(&BQlist[2]) && GQis_empty(&BQlist[3]) && GQis_empty(&BQlist[4]) && GQis_empty(&BQlist[5]) && GQis_empty(&BQlist[6]))){//while loop that goes till all queues are empty
            for(int i=0; i<7; i++){
                if (GQis_empty(&BQlist[i])) {
                    continue;
                }
                //for(int j=0; j<25; j++){  //uncommenting this line and the line 10 lines down might make it less jagged? (NOTE: jaggedness is possibly due to the fact that the characters are horizontally closter together than they are vertically)
                if(!GQdequeue_RC(&BQlist[i], &curRow, &curCol)){   //check to see if current queue is empty
                    if (Tgrid[curRow][curCol] == TT_NO_TERRAIN){                            //if the grid space isn't already occupied, make it the correct biome and add its neighbors. otherwise just ignore it and move on
                        Tgrid[curRow][curCol] = biomesList[i];                    //make it the correct biome
                        if(curRow-1 >= 0){ GQenqueue(&BQlist[i], curRow-1, curCol, precedence++, NULL); }   //check if space above seed is in bounds
                        if(curRow+1 <=20){ GQenqueue(&BQlist[i], curRow+1, curCol, precedence++, NULL); }   //checks if space below seed is in bounds
                        if(curRow-1 >= 0){ GQenqueue(&BQlist[i], curRow, curCol-1, precedence++, NULL); }   //checks if space to left of seed is in bounds
                        if(curRow+1 <=79){ GQenqueue(&BQlist[i], curRow, curCol+1, precedence++, NULL); }   //checks if space to right of seed is in bounds
                    }
                }
            //}
            }
        }
        return 0;

    }

    int generatePaths(){
        //creating the exits on the map and a path 1 in from the boarder
        if(gateN != -1){
            Tgrid[0][gateN] = TT_GATE;
            Tgrid[1][gateN] = TT_PATH;
        }else{
            gateN = 40;                  //if you are at the top of the map, the path will lead to the middle of the top but not break the wall
        }
        if(gateS != -1){
            Tgrid[20][gateS] = TT_GATE;
            Tgrid[19][gateS] = TT_PATH;
        }else{
            gateS = 40;                 //if you are at the bottom of the map, the path will lead to the middle of the bottom but not break the wall
        }
        if(gateW != -1){
            Tgrid[gateW][0] = TT_GATE;
            Tgrid[gateW][1] = TT_PATH;
        }else{
            gateW = 10;                  //if you are at the left  of the map, the path will lead to the middle of the west but not break the wall
        }
        if(gateE != -1){
            Tgrid[gateE][79] = TT_GATE;
            Tgrid[gateE][78] = TT_PATH;
        }else{
            gateE = 10;                  //if you are at the far right  of the map, the path will lead to middle of east but not break wall
        }
        
        PathProb pp;
        int curRow;
        int curCol;
        int randDir;

        //make connection from N->S
        curRow = 1;
        curCol = gateN;
        initPathProb(&pp, 1, gateN, 19, gateS); //going from col 1 to col 19 (instead 0 to 20) to avoid breaking boundry anywhere besides the exit cell
        while((pp.numV + pp.numH) > 0){
            randDir = rand() % (pp.numV + pp.numH);
            if(randDir < pp.numH){
                //go H one unit in correct direction & decrease the H count
                curCol += pp.dirH;
                pp.numH--;
            }else{
                //go V one unit in correct direction & decrease V count
                curRow += pp.dirV;
                pp.numV--;
            }
            //update the caracter at the given space. Make it a bridge if it is water, otherwise make it a path
            if (Tgrid[curRow][curCol] != TT_WATER){
                Tgrid[curRow][curCol] = TT_PATH;
            }else{
                Tgrid[curRow][curCol] = TT_BRIDGE;
            }
        
        }

        //make connection from W->E
        initPathProb(&pp, gateW, 1, gateE, 78); //going from 1 to 78 (instead of 0 to 79) to avoid breaking boundry anywhere besides the exit cell.
        curRow = gateW;
        curCol = 1;
        while((pp.numV + pp.numH) > 0){
            randDir = rand() % (pp.numV + pp.numH);
            if(randDir < pp.numH){
                //go H one unit in correct direction & decrease the H count
                curCol += pp.dirH;
                pp.numH--;
            }else{
                //go V one unit in correct direction & decrease V count
                curRow += pp.dirV;
                pp.numV--;
            }
            //update the caracter at the given space. Make it a bridge if it is water, otherwise make it a path
            if (Tgrid[curRow][curCol] != TT_WATER){
                Tgrid[curRow][curCol] = TT_PATH;
            }else{
                Tgrid[curRow][curCol] = TT_BRIDGE;
            }
        }
        return 0;
    }

    int generateBuildings(){
        int randRow;
        int randCol;
        int distance = abs(globalRow - 200) + abs(globalCol - 200);       //Manhattan distance from center of board
        int randSpawnNum = rand() % 400; //generates a random number from 0 to 399

        if (randSpawnNum >= distance){ 
            //TO PLACE THE POKEMART
            while (1){
                randRow = rand() % 14 + 3;
                randCol = rand() % 73 + 3;

                //check if the pokeMart can be placed without overlapping a road or bridge but still touching a road. (placed up-right of path)
                if(Tgrid[randRow][randCol] == TT_PATH && Tgrid[randRow][randCol+1] != TT_PATH && Tgrid[randRow][randCol+2] != TT_PATH && Tgrid[randRow-1][randCol+1] != TT_PATH && Tgrid[randRow-1][randCol+2] != TT_PATH &&
                                                            Tgrid[randRow][randCol+1] != TT_BRIDGE && Tgrid[randRow][randCol+2] != TT_BRIDGE && Tgrid[randRow-1][randCol+1] != TT_BRIDGE && Tgrid[randRow-1][randCol+2] != TT_BRIDGE){
                    Tgrid[randRow][randCol+1] = TT_PMART;
                    Tgrid[randRow][randCol+2] = TT_PMART;
                    Tgrid[randRow-1][randCol+1] = TT_PMART;
                    Tgrid[randRow-1][randCol+2] = TT_PMART;
                    break;
                }

                //check if the pokeMart can be placed without overlapping a road or bridge but still touching a road. (placed up-left of path)
                if(Tgrid[randRow][randCol] == TT_PATH && Tgrid[randRow][randCol-1] != TT_PATH && Tgrid[randRow][randCol-2] != TT_PATH && Tgrid[randRow-1][randCol-1] != TT_PATH && Tgrid[randRow-1][randCol-2] != TT_PATH &&
                                                            Tgrid[randRow][randCol-1] != TT_BRIDGE && Tgrid[randRow][randCol-2] != TT_BRIDGE && Tgrid[randRow-1][randCol-1] != TT_BRIDGE && Tgrid[randRow-1][randCol-2] != TT_BRIDGE){

                    Tgrid[randRow][randCol-1] = TT_PMART;
                    Tgrid[randRow][randCol-2] = TT_PMART;
                    Tgrid[randRow-1][randCol-1] = TT_PMART;
                    Tgrid[randRow-1][randCol-2] = TT_PMART;
                    break;
                }
            }
        }

        randSpawnNum = rand() % 400; // generates a random number from 0 to 399
        if (randSpawnNum >= distance){
            //TO PLACE THE POKECENTER
            while (1){
                randRow = rand() % 14 + 3;
                randCol = rand() % 73 + 3;
                //the following if statement has to make sure it doesn't overlap a path OR a pokemart that has already been placed
                if(Tgrid[randRow][randCol] == TT_PATH &&
                Tgrid[randRow][randCol+1] != TT_PATH && Tgrid[randRow][randCol+1] != TT_PMART && Tgrid[randRow][randCol+1] != TT_BRIDGE && 
                Tgrid[randRow][randCol+2] != TT_PATH && Tgrid[randRow][randCol+2] != TT_PMART && Tgrid[randRow][randCol+2] != TT_BRIDGE &&
                Tgrid[randRow-1][randCol+1] != TT_PATH && Tgrid[randRow-1][randCol+1] != TT_PMART && Tgrid[randRow-1][randCol+1] != TT_BRIDGE &&
                Tgrid[randRow-1][randCol+2] != TT_PATH && Tgrid[randRow-1][randCol+2] != TT_PMART && Tgrid[randRow-1][randCol+2] != TT_BRIDGE){

                    Tgrid[randRow][randCol+1] = TT_PCENTER;
                    Tgrid[randRow][randCol+2] = TT_PCENTER;
                    Tgrid[randRow-1][randCol+1] = TT_PCENTER;
                    Tgrid[randRow-1][randCol+2] = TT_PCENTER;
                    break;
                }
                if(Tgrid[randRow][randCol] == TT_PATH &&
                Tgrid[randRow][randCol-1] != TT_PATH && Tgrid[randRow][randCol-1] != TT_PMART && Tgrid[randRow][randCol-1] != TT_BRIDGE &&
                Tgrid[randRow][randCol-2] != TT_PATH && Tgrid[randRow][randCol-2] != TT_PMART && Tgrid[randRow][randCol-2] != TT_BRIDGE &&
                Tgrid[randRow-1][randCol-1] != TT_PATH && Tgrid[randRow-1][randCol-1] != TT_PMART && Tgrid[randRow-1][randCol-1] != TT_BRIDGE &&
                Tgrid[randRow-1][randCol-2] != TT_PATH && Tgrid[randRow-1][randCol-2] != TT_PMART && Tgrid[randRow-1][randCol-2] != TT_BRIDGE){

                    Tgrid[randRow][randCol-1] = TT_PCENTER;
                    Tgrid[randRow][randCol-2] = TT_PCENTER;
                    Tgrid[randRow-1][randCol-1] = TT_PCENTER;
                    Tgrid[randRow-1][randCol-2] = TT_PCENTER;
                    break;
                }
            }
        }
        return 0;
    }

    int placeRandomTerrain(){
        srand(time(NULL));
        int randNum;      //variable to determine whether you should place a random object (every number is 1% chance)
        char curObj;        //variable to store what biome the cell you are think of placing an object is
        for(int i=1; i<20; i++){        //iterates through all spaces on the board (except for boarder)
            for(int j=1; j<79; j++){
                randNum = rand() % 150;       //generate random number from 0-99 that will give probability of placing object
                curObj = Tgrid[i][j];     //getting what object is at the current location
                if ((randNum == 13 || randNum == 7) && (curObj == TT_SGRASS|| curObj == TT_TGRASS)){                     //if number is 13, place a TT_TREE (~1% chance). only allowed to be placed on short or tall grass
                    Tgrid[i][j] = TT_TREE;
                }
                if (randNum == 69 && (curObj == TT_FOREST || curObj == TT_SGRASS || curObj == TT_TGRASS)){    //if number is 69, place a rock (~1% chance). only allowed to be placed in TT_FOREST, short and tall grass
                    Tgrid[i][j] = TT_BOULDER;
                }
            }
        }
    
        return 0;
    }

    int generateCatacombEntrance(){
        int cRow;
        int cCol;
        do{
            cRow = rand()%15 + 3;  //generates a random row from 3 to 17 (makes sure you are 2 blocks from border of map)
            cCol = rand()%74 + 3; //generates a random col from 3 to 76 (makes sure you are 2 blocks frmo border of map)
        }while(Tgrid[cRow][cCol] != TT_SGRASS && Tgrid[cRow][cCol] != TT_TGRASS); //only choose those coords if they are player reachable
        catacombRow = cRow;
        catacombCol = cCol;
        //generate graves around the catacomb (in a diamond shape) making sure to not place one on a pokecenter, pokemart, Path, or Bridge. Also, probability of graves spawning is higher near the entrance (last condition of the for loop)
        //WARNING: IF YOU CHANGE THE SIZE OF THE GRAVEYARD, MAKE SURE TO SPAWN THEM FARTHER FROM THE BORDER (adjust the random numbers in the do while loop above)
        for(int i = -2; i<=2; i++){
            for(int j = -2; j<=2; j++){
                if(Tgrid[cRow+i][cCol+j] != TT_PMART && Tgrid[cRow+i][cCol+j] != TT_PCENTER && Tgrid[cRow+i][cCol+j] != TT_BRIDGE && Tgrid[cRow+i][cCol+j] != TT_PATH && rand()%4 <= 4-abs(j)-abs(i)){
                    Tgrid[catacombRow + i][catacombCol + j] = TT_GRAVE;
                }
            }
        }
        //place the catacomb entrance into the map AFTER generating the graves so you don't place one on it
        Tgrid[catacombRow][catacombCol] = TT_CATACOMB;
        return 0;
    }

    int printBoard(){
        for(int i=0; i<21; i++){
            for(int j=0; j<80; j++){
                if (charGrid[i][j] != NULL){ //if there is a character at the given spot, print the character
                    int curChar = charGrid[i][j]->type;
                    attron(COLOR_PAIR(COLOR_RED));
                    if(curChar < 0 || curChar >= CT_OTHER){
                        mvprintw(0,0,"ERROR: invalid character type at row:%d col:%d)", i,j); break;
                    }else{
                        mvprintw(i+1,j,"%c", CT_to_Char(curChar));
                    }
                    attroff(COLOR_PAIR(COLOR_RED)); 
                } else {    //else print the terrain
                    int curTerrain = Tgrid[i][j];
                    switch(curTerrain){
                        case TT_PCENTER:
                            attron(COLOR_PAIR(COLOR_MAGENTA)); 
                            mvprintw(i+1,j,"C");
                            attroff(COLOR_PAIR(COLOR_MAGENTA));
                            break;
                        case TT_PMART:
                            attron(COLOR_PAIR(COLOR_MAGENTA));   
                            mvprintw(i+1,j,"P"); 
                            attroff(COLOR_PAIR(COLOR_MAGENTA));
                            break;
                        case TT_SGRASS:
                            attron(COLOR_PAIR(COLOR_GREEN));  
                            mvprintw(i+1,j,"."); 
                            attroff(COLOR_PAIR(COLOR_GREEN));
                            break;
                        case TT_TGRASS:
                            attron(COLOR_PAIR(COLOR_GREEN));
                            mvprintw(i+1,j,":"); 
                            attroff(COLOR_PAIR(COLOR_GREEN));
                            break;
                        case TT_WATER:
                            attron(COLOR_PAIR(COLOR_CYAN));
                            mvprintw(i+1,j,"~");
                            attroff(COLOR_PAIR(COLOR_CYAN)); 
                            break;
                        case TT_PATH:    //paths are '#' so this continues
                        case TT_GATE:    //gates are '#' so this continues
                        case TT_BRIDGE:  
                            attron(COLOR_PAIR(COLOR_YELLOW));
                            mvprintw(i+1,j,"#"); 
                            attroff(COLOR_PAIR(COLOR_YELLOW));
                            break;
                        case TT_TREE:    //trees are '^' so this continues
                        case TT_FOREST: 
                            attron(COLOR_PAIR(COLOR_GREEN)); 
                            mvprintw(i+1,j,"^");
                            attroff(COLOR_PAIR(COLOR_GREEN));
                            break;
                        case TT_BOULDER: //boulders are '%' so this continues
                        case TT_MOUNTAIN:mvprintw(i+1,j,"%%"); break;
                        case TT_GRAVE: mvprintw(i+1,j, "+"); break;
                        case TT_CATACOMB: mvprintw(i+1,j, " "); break;
                        case TT_NO_TERRAIN: mvprintw(i+1,j,"0"); break;                        
                        default: mvprintw(0,0,"ERROR: invalid terrain type"); break;
                    }
                }
                //printf(" ");        puts a space between each of the characters
            }
            //printf("\n");  no need for this with ncurses
        }
        //mvprintw(22,0,"0         1         2         3         4         5         6         7         8"); //DELETETHIS
        refresh();
        return 0;
    }

    //DECONSTRUCTOR: TODO
    ~Map(){};

};

class NPC: public Character{
public:
    int direction;
    int isDefeated;

    NPC(int NPCtype, Map* m, WorldMap* WM, PC* player, int wRow, int wCol, int playerPokeCount){
        int randRow;
        int randCol;
        do{
            //generate random (row,col)
            randRow = rand() % 19 + 1;       //generates a row within the border of the world (1 to 19)
            randCol = rand() % 78 + 1;       //generates a col within the border of the world (1 to 78)
        }while (calc_Cost(m->Tgrid[randRow][randCol], NPCtype) == MAX || m->charGrid[randRow][randCol] != NULL);    //keep generating random points while the random spot has unsuitable terrain or already has a character
        type = NPCtype;
        row = randRow;
        col = randCol;

        //set direction
        if (NPCtype == CT_WANDERER || NPCtype == CT_PACER || NPCtype == CT_EXPLORER){
            direction = rand()%4 ;  //generate a random direction 0=NORTH to 3= WEST
        }else{
            direction = NO_DIRECTION;
        }

        //set next position
        findNextPos(this, m, WM, player);

        isDefeated = 0;  //makes the NPCs initially not defeated
        m->charGrid[row][col] = this; //put new Character into the Map at the correct spot

        //generate the NPC's pokemon
        int numPokemon = rand() % 10;
        if(numPokemon < 6){ //there is a 60% chance the NPC will have one more pokemon than the player
            for(int i=0; i < playerPokeCount+1 && i<6; i++){ //make sure to not give the NPC more than 6 pokemon
                pokemon.push_back(InGamePokemon(wRow, wCol));
            }
        }else{ //otherwise, give the trainer a random number of pokemon between 1 and the number of pokemon the player has [also can't go over 6]
            numPokemon = rand() % playerPokeCount + 1;
            for (int i=0; i<numPokemon && i<6; i++){
                pokemon.push_back(InGamePokemon(wRow, wCol));
            }
        }

        GQenqueue(&(WM->charQueue), row, col, nextCost, this); //enqueue the character  
    } 

    int moveOneDir(Map* m){
        if(direction == NORTH){
            this->updateNextCoords(row-1, col);
            nextCost = calc_Cost(m->Tgrid[row - 1][col], type);
        }
        else if (direction == EAST){
            this->updateNextCoords(row, col+1);
            nextCost = calc_Cost(m->Tgrid[row][col + 1], type);
        }
        else if (direction == SOUTH){
            this->updateNextCoords(row+1, col);
            nextCost = calc_Cost(m->Tgrid[row + 1][col], type);
        }
        else if (direction == WEST){
            this->updateNextCoords(row, col-1);
            nextCost = calc_Cost(m->Tgrid[row][col - 1], type);
        }
        return 0;
    }

    ~NPC(){}

};

class PC: public Character{
public:
    int w_row;
    int w_col;
    int inCatacombs;
    int numPokeballs;
    int numPotions;
    int numRevives;

    //constructor to initialize the PC at a random road coordinate on the map
    PC(WorldMap* WM, Map startMap, int start_w_row, int start_w_col){
        type = CT_PLAYER;
        int randRow = (rand() % 19) + 1;         //generates row from 1 to 20 so you don't spawn in gate
        int randCol = (rand() % 78) + 1;         //generates col from 1 to 79 so you don't spawn in gate
        while(startMap.Tgrid[randRow][randCol] != TT_PATH){   //if you aren't on a road, get another random position
            randRow = (rand() % 19) + 1;
            randCol = (rand() % 78) + 1;
        }
        row = randRow;
        col = randCol;
        nextRow = randRow;
        nextCol = randCol;
        nextCost = calc_Cost(startMap.Tgrid[randRow][randCol], CT_PLAYER);
        w_row = start_w_row;
        w_col = start_w_col;

        WM->player = this;  //put player into world map
        inCatacombs = 0; //player should not start in the catacombs

        //GIVE THE PLAYER ITEMS
        numPokeballs = 5;
        numPotions = 10;
        numRevives = 3;

        //SELECT THE STARTER POKEMON
        std::vector<InGamePokemon> pokemonToChoose; //an array to store the 3 possible pokemon to choose from
        for(int i=0; i<3; i++){
            pokemonToChoose.push_back(InGamePokemon(200,200));
        }
        //print the choices to the user
        createPanel(3,15,12,68);
        mvprintw(4, 17, "Choose your starter pokemon!");
        mvprintw(5, 17, "1. %s", pokemonToChoose[0].name.c_str());
        mvprintw(6, 17, "2. %s", pokemonToChoose[1].name.c_str());
        mvprintw(7, 17, "3. %s", pokemonToChoose[2].name.c_str());
        char usrChar;
        do{
            usrChar = getch();
        }while(usrChar != '1' && usrChar != '2' && usrChar != '3');
        //give the user the pokemon they chose
        switch(usrChar){
            case '1':
                pokemon.push_back(pokemonToChoose[0]);
                break;
            case '2':
                pokemon.push_back(pokemonToChoose[1]);
                break;
            case '3':
                pokemon.push_back(pokemonToChoose[2]);
                break;
        }
    }

    //function to display the players back. "location" is where they are calling it from: 1 = map, 2 = battle with trainer, 3 = battle with wild pokemon
    //this function returns 3 if you use a pokeball and 0 otherwise. In the future I could make it return 0 after each use of a potion if you are in battle (location == 1||3)
    int openBag(int location){
        createPanel(4, 18, 10, 69);
        int bagKey;
        int pokeChoice;
        do{
            mvprintw(5, 13, "welcome to your sack: press ESC to leave");
            //print items
            mvprintw(6, 13, "1. potion    [x%d]               ", numPotions);
            mvprintw(7, 13, "2. revive    [x%d]               ", numRevives);
            mvprintw(8, 13, "3. pokeballs [x%d]               ", numPokeballs);
            //print pokemon
            for (long unsigned int i = 0; i < pokemon.size(); i++){
                mvprintw(11+i, 13, "%ld. %s  [HP: %d/%d]   [Lvl:%d]    ", i+1, pokemon[i].name.c_str(), pokemon[i].curHealth, pokemon[i].health, pokemon[i].level);
            }

            mvprintw(10,13, "Choose an item to use                           ");
            bagKey = getch();
            if (bagKey == '1' && numPotions > 0){
                mvprintw(10,13, "Choose a pokemon to use the potion on:               ");
                pokeChoice = getch()-'0';
                while(pokeChoice <= 0 || pokeChoice > int(pokemon.size())){
                    mvprintw(10, 13, "invalid input. try again:                             ");
                    pokeChoice = getch()-'0';
                }
                pokemon[pokeChoice-1].curHealth += 20;
                if (pokemon[pokeChoice-1].curHealth > pokemon[pokeChoice-1].health){
                    pokemon[pokeChoice-1].curHealth = pokemon[pokeChoice-1].health;
                }
                numPotions--;
                createPanel(4, 18, 10, 69);
                mvprintw(10, 13, "you used a potion on %s            ", pokemon[pokeChoice-1].name.c_str());                
                getch();
            }

            else if (bagKey == '2' && numRevives > 0){
                if (numLivePokemon() == int(pokemon.size())){
                    mvprintw(10, 13, "all your pokemon are alive! none can be revived.  ");
                    getch();
                    createPanel(4, 18, 10, 69);
                }else{
                    mvprintw(5,13, "Choose a pokemon to Revive:                       ");
                    pokeChoice = getch()-'0';
                    while(pokeChoice <= 0 || pokeChoice > int(pokemon.size()) || pokemon[pokeChoice-1].curHealth != 0){
                        if(pokeChoice <= 0 || pokeChoice > int(pokemon.size()))
                            mvprintw(10, 13, "invalid input. try again:                             ");
                        else if(pokemon[pokeChoice-1].curHealth != 0){
                            mvprintw(10, 13, "That pokemon is not fainted. try again:                ");
                        }
                        pokeChoice = getch()-'0';
                    }
                    pokemon[pokeChoice-1].curHealth = pokemon[pokeChoice-1].health/2; //revive restores 50% of health
                    numRevives--;
                    mvprintw(10,13, "you used a revived %s                     ", pokemon[pokeChoice-1].name.c_str());
                    getch();
                }
            }

            else if (bagKey == '3'){
                if (location != 3){
                    createPanel(4, 18, 10, 69);
                    mvprintw(10, 13, "you cannot use pokeballs here!              ");
                }else if(numPokeballs > 0){
                    if(pokemon.size() >= 6){
                        mvprintw(10, 13, "Cannot have more than 6 pokemon             ");
                    }else{
                        numPokeballs--;
                        return 3; //this means you used a pokeball
                    }
                }
                getch();
            }
            else if (bagKey != KEY_ESC){
                createPanel(4, 18, 10, 69);
                mvprintw(10, 13, "invalid input. Enter an item 1-3              ");
                getch();
            }
        }while (bagKey != KEY_ESC);
        return 0;
    }

    int battle(Map* m, NPC* opponent){
        int battleOver = 0;
        int usrKey;
        int curPCpokemon = 0;
        int curNPCpokemon = 0;
        int skipPCturn = 0;
        int skipNPCturn = 0;
        Moves curPCmove;
        Moves curNPCmove;


        
        while(!battleOver){
            skipPCturn = 0; //make sure you don't skip the PC turn unless they use a move that skips their turn
            skipNPCturn = 0;
            createPanel(3, 19,6, 73); //midpoint is 11, 40
            mvprintw(4, 12, "You are battling [%c] [%d pokemon left]", CT_to_Char(opponent->type), opponent->numLivePokemon());

            //Print Player Pokemon
            mvprintw(6, 8, "PC POKEMON: %s  ", pokemon[curPCpokemon].name.c_str());
            mvprintw(7, 8, "LVL: %d", pokemon[curPCpokemon].level);
            mvprintw(7, 17, "HP: %d/%d     ", pokemon[curPCpokemon].curHealth, pokemon[curPCpokemon].health);
            mvprintw(8, 8, "ATTACK: %d    S[%d]", pokemon[curPCpokemon].attack, pokemon[curPCpokemon].special_attack);
            mvprintw(9, 8, "DEFENSE: %d    S[%d]", pokemon[curPCpokemon].defense, pokemon[curPCpokemon].special_defense);
            mvprintw(10,8, "SPEED: %d", pokemon[curPCpokemon].speed);
            createPanel(11,19,6,73);
            //mvprintw(11,7, "%s,%s     %s,%s", movesVector[pokemon[0].moves[0].move_id].identifier.c_str(), movesVector[pokemon[0].moves[1].move_id].identifier.c_str(), movesVector[opponent->pokemon[0].moves[0].move_id].identifier.c_str(), movesVector[opponent->pokemon[0].moves[1].move_id].identifier.c_str()); //FORTESTING
            
            //Print Opponent Pokemon
            mvprintw(6, 40, "NPC POKEMON: %s  ", opponent->pokemon[curNPCpokemon].name.c_str());
            mvprintw(7, 40, "LVL: %d", opponent->pokemon[curNPCpokemon].level);
            mvprintw(7, 50, "HP: %d/%d     ", opponent->pokemon[curNPCpokemon].curHealth, opponent->pokemon[curNPCpokemon].health);
            mvprintw(8, 40, "ATTACK: %d    S[%d]", opponent->pokemon[curNPCpokemon].attack, opponent->pokemon[curNPCpokemon].special_attack);
            mvprintw(9, 40, "DEFENSE: %d    S[%d]", opponent->pokemon[curNPCpokemon].defense, opponent->pokemon[curNPCpokemon].special_defense);
            mvprintw(10,40, "SPEED: %d", opponent->pokemon[curNPCpokemon].speed);

            //GET USR INPUT
            clearPanel(12, 18, 7, 72);
            //the following loop will run until the user enters a valid input
            while (1){
                 mvprintw(12, 10, "Choose Action:   1.Fight    2.Bag    3.RUN    4.Pokemon");
                //see if player is fully dead.
                if (numLivePokemon() == 0){
                    mvprintw(18, 10, "You have no pokemon left. You lose.");
                    getch();
                    mvprintw(0,0, "Player Lost");
                    return 0;
                }
                
                //see if player's current pokemon is dead. if so, they are forced to choose another
                if (pokemon[curPCpokemon].curHealth<=0){
                    createPanel(11,19,6,73);
                    mvprintw(12, 10, "Your previous pokemon fainted. Choose another");
                    usrKey = '4';
                    getch();
                }else{
                    usrKey = getch();
                }

                createPanel(11,19,6,73);
                if (usrKey == '1'){ //FIGHT
                    //print the moves the user can use
                    mvprintw(12, 10, "Choose a move:");
                    for(long unsigned int i=0; i<pokemon[curPCpokemon].moves.size(); i++){
                        mvprintw(13+i, 10, "%ld. %s", i+1, pokemon[curPCpokemon].moves[i].identifier.c_str());
                    }
                    //ask the user to enter a number
                    while (1){
                        usrKey = getch();
                        if (usrKey-48 > int(pokemon[curPCpokemon].moves.size()) || usrKey-48<=0){ //we have to do usrKey-48 because getch() returns the ASCII value of the key pressed [0=48,1=49,...,9=57]
                            mvprintw(18, 10, "Invalid input. Please enter a number from 1 to %ld", pokemon[curPCpokemon].moves.size());
                        }else{
                            curPCmove = pokemon[curPCpokemon].moves[usrKey-48-1];      //note the move that the PC will use (-48 converts to integer from ASCII and -1 accounts for the fact that arrays index from 0)
                            //PCpriority = movesVector[curPCmove.move_id].priority;   //set the priority of the PC's move
                            break;
                        }
                    }
                    break;
                }else if (usrKey == '2'){ //BAG
                    //Open your bag and do stuff. You shouldn't be able to use pokeballs here so no need to keep track of return val
                    openBag(2);
                    //REMOVE BAG FROM SCREEN
                    createPanel(3, 19,6, 73); //midpoint is 11, 40
                    mvprintw(4, 12, "you are battling [%c] [%ld pokemon]", CT_to_Char(opponent->type), opponent->pokemon.size());

                    //Print Player Pokemon
                    mvprintw(6, 8, "PC POKEMON: %s  ", pokemon[curPCpokemon].name.c_str());
                    mvprintw(7, 8, "LVL: %d", pokemon[curPCpokemon].level);
                    mvprintw(7, 17, "HP: %d/%d     ", pokemon[curPCpokemon].curHealth, pokemon[curPCpokemon].health);
                    mvprintw(8, 8, "ATTACK: %d    S[%d]", pokemon[curPCpokemon].attack, pokemon[curPCpokemon].special_attack);
                    mvprintw(9, 8, "DEFENSE: %d    S[%d]", pokemon[curPCpokemon].defense, pokemon[curPCpokemon].special_defense);
                    mvprintw(10,8, "SPEED: %d", pokemon[curPCpokemon].speed);
                    createPanel(11,19,6,73);
                    
                    //Print Opponent Pokemon
                    mvprintw(6, 40, "NPC POKEMON: %s  ", opponent->pokemon[curNPCpokemon].name.c_str());
                    mvprintw(7, 40, "LVL: %d", opponent->pokemon[curNPCpokemon].level);
                    mvprintw(7, 50, "HP: %d/%d     ", opponent->pokemon[curNPCpokemon].curHealth, opponent->pokemon[curNPCpokemon].health);
                    mvprintw(8, 40, "ATTACK: %d    S[%d]", opponent->pokemon[curNPCpokemon].attack, opponent->pokemon[curNPCpokemon].special_attack);
                    mvprintw(9, 40, "DEFENSE: %d    S[%d]", opponent->pokemon[curNPCpokemon].defense, opponent->pokemon[curNPCpokemon].special_defense);
                    mvprintw(10,40, "SPEED: %d", opponent->pokemon[curNPCpokemon].speed);
                    skipPCturn = 1;
                    break;
                }else if (usrKey == '3'){ //if the user tries to run, they have a 75% chance of success
                    if (rand() % 4 != 0){
                        mvprintw(0, 0, "You successfully ran away! (pussy)");
                        return 0;
                    }else{
                        mvprintw(18, 10, "You failed to run away!");
                        getch();
                        skipPCturn = 1;
                    }
                }else if (usrKey == '4'){ //SWITCH POKEMON
                    
                    mvprintw(12, 10, "Choose a pokemon:");
                    for(long unsigned int i=0; i<pokemon.size(); i++){
                        mvprintw(13+i, 10, "%ld. %s  HP: %d/%d", i+1, pokemon[i].name.c_str(), pokemon[i].curHealth, pokemon[i].health);
                    }
                    //get user input
                    do{
                        usrKey = getch();
                        //createPanel(11,19,6,73);
                        if (usrKey-48 > int(pokemon.size()) || usrKey-48<=0){
                            mvprintw(18, 10, "Invalid input. Please enter a number from 1 to %ld", pokemon.size());
                        }else if(pokemon[usrKey-48-1].curHealth == 0){
                            mvprintw(18, 10, "That pokemon is fainted. Please choose another pokemon.");
                        }
                    }while(usrKey-48 > int(pokemon.size()) || usrKey-48<=0 || pokemon[usrKey-48-1].curHealth == 0);
                    curPCpokemon = usrKey-48-1;
                    createPanel(11,19,6,73);
                    mvprintw(18, 10, "PC switched to %s", pokemon[curPCpokemon].name.c_str());
                    getch();
                    skipPCturn = 1;
                    break;
                }else{
                    mvprintw(18, 10, "Invalid input. Please enter a number from 1 to 4");
                }
            }

            //GEN NPC's ACTION
            if (opponent->pokemon[curNPCpokemon].curHealth <= 0){ //health should never be less than 0, but just in case
                curNPCpokemon += 1;
                createPanel(11,19,6,73);
                mvprintw(18, 10, "NPC switched to %s", (opponent->pokemon[curNPCpokemon].name).c_str());
                getch();
                skipNPCturn = 1;
            }else{
                //gen random move
                curNPCmove = opponent->pokemon[curNPCpokemon].moves[rand() % opponent->pokemon[curNPCpokemon].moves.size()];
            }
            
            //CARRY OUT ACTIONS
            if(!skipPCturn && !skipNPCturn){ //if both people go
                //find who goes first
                if (!skipNPCturn && (10000 * curPCmove.priority + 2 * pokemon[curPCpokemon].speed + rand()%2 >= 10000*curNPCmove.priority + 2*opponent->pokemon[curNPCpokemon].speed + rand()%2)){
                    
                    //PC goes first
                    attack(&pokemon[curPCpokemon], &curPCmove, &(opponent->pokemon[curNPCpokemon]), "PLAYER"); //have the player carry out their move

                    //NPC goes second: if the NPC is killed, then his turn is skipped, but otherwise they do their attack
                    if(opponent->pokemon[curNPCpokemon].curHealth <= 0){
                        opponent->pokemon[curNPCpokemon].curHealth = 0;
                        createPanel(11,19,6,73);
                        mvprintw(18, 10, "NPC's %s fainted!", (opponent->pokemon[curNPCpokemon].name).c_str());
                        getch();
                    }else{                                             
                        attack(&opponent->pokemon[curNPCpokemon], &curNPCmove, &pokemon[curPCpokemon], "NPC"); //have the NPC carry out their move
                    }
                }
                else{

                    //NPC goes first
                    attack(&opponent->pokemon[curNPCpokemon], &curNPCmove, &pokemon[curPCpokemon], "NPC"); //have the NPC carry out their move

                    //PC goes second: if the PC is killed, then his turn is skipped, but otherwise they do their attack
                    if(pokemon[curPCpokemon].curHealth <= 0){
                        pokemon[curPCpokemon].curHealth = 0;
                        createPanel(11,19,6,73);
                        mvprintw(18, 10, "PC's %s fainted!", (pokemon[curPCpokemon].name).c_str());
                        getch();
                    }else{
                        attack(&pokemon[curPCpokemon], &curPCmove, &(opponent->pokemon[curNPCpokemon]), "PLAYER"); //have the player carry out their move
                    }
                }
            }else if(!skipPCturn){
                //only PC attacks
                attack(&pokemon[curPCpokemon], &curPCmove, &(opponent->pokemon[curNPCpokemon]), "PLAYER"); //have the player carry out their move
            }else if(!skipNPCturn){
                //only NPC attacks
                attack(&opponent->pokemon[curNPCpokemon], &curNPCmove, &pokemon[curPCpokemon], "NPC"); //have the NPC carry out their move
            }else{
                //no one battles
            }

            //CHECK FOR DEATH
            //check PC death
            if (numLivePokemon() == 0){
                mvprintw(0,0, "Player Lost");
                battleOver = 1; //maybe don't need this
                return 0;
            }

            //check NPC death
            if(opponent->numLivePokemon()==0){
                mvprintw(0,0, "Player wins!");
                opponent->isDefeated = 1;
                m->charGrid[opponent->row][opponent->col] = NULL; //remove the character from the charMap
                battleOver = 1; //maybe dont need this
                return 0;
            }
        }
        return 0;
    }

    //This will be the battle function called on wild pokemon
    int battleWildPokemon(InGamePokemon* wildPokemon){
        int battleOver = 0;
        int usrKey;
        int curPCpokemon = 0;
        int skipPCturn = 0;
        Moves curPCmove;
        Moves curWildMove;


        
        while(!battleOver){
            skipPCturn = 0; //make sure you don't skip the PC turn unless they use a move that skips their turn
            createPanel(3, 19,6, 73); //midpoint is 11, 40
            mvprintw(4, 12, "you encountered a wild %s",wildPokemon->name.c_str());
            //Print Player Pokemon
            mvprintw(6, 8, "PC POKEMON: %s  ", pokemon[curPCpokemon].name.c_str());
            mvprintw(7, 8, "LVL: %d", pokemon[curPCpokemon].level);
            mvprintw(7, 17, "HP: %d/%d     ", pokemon[curPCpokemon].curHealth, pokemon[curPCpokemon].health);
            mvprintw(8, 8, "ATTACK: %d    S[%d]", pokemon[curPCpokemon].attack, pokemon[curPCpokemon].special_attack);
            mvprintw(9, 8, "DEFENSE: %d    S[%d]", pokemon[curPCpokemon].defense, pokemon[curPCpokemon].special_defense);
            mvprintw(10,8, "SPEED: %d", pokemon[curPCpokemon].speed);
            createPanel(11,19,6,73);
            
            //Print Opponent Pokemon
            mvprintw(6, 40, "WILD POKEMON: %s  ", wildPokemon->name.c_str());
            mvprintw(7, 40, "LVL: %d", wildPokemon->level);
            mvprintw(7, 50, "HP: %d/%d     ", wildPokemon->curHealth, wildPokemon->health);
            mvprintw(8, 40, "ATTACK: %d    S[%d]", wildPokemon->attack, wildPokemon->special_attack);
            mvprintw(9, 40, "DEFENSE: %d    S[%d]", wildPokemon->defense, wildPokemon->special_defense);
            mvprintw(10,40, "SPEED: %d", wildPokemon->speed);

            //GET USR INPUT
            clearPanel(12, 18, 7, 72);
            mvprintw(12, 10, "Choose Action:   1.Fight    2.Bag    3.RUN    4.Pokemon");
            //the following loop will run until the user enters a valid input
            while (1){
                //see if player is fully dead.
                if (numLivePokemon()==0){
                    mvprintw(18, 10, "You have no pokemon left. You lose.");
                    getch();
                    mvprintw(0,0, "Player Lost");
                    return 0;
                }
                
                //see if player's current pokemon is dead. if so, they are forced to choose another
                if (pokemon[curPCpokemon].curHealth<=0){
                    createPanel(11,19,6,73);
                    mvprintw(12, 10, "Your previous pokemon fainted. Choose another");
                    usrKey = '4';
                    getch();
                }else{
                    usrKey = getch();
                }

                createPanel(11,19,6,73);
                if (usrKey == '1'){ //FIGHT
                    //print the moves the user can use
                    mvprintw(12, 10, "Choose a move:");
                    for(long unsigned int i=0; i<pokemon[curPCpokemon].moves.size(); i++){
                        mvprintw(13+i, 10, "%ld. %s", i+1, pokemon[curPCpokemon].moves[i].identifier.c_str());
                    }
                    //ask the user to enter a number
                    while (1){
                        usrKey = getch();
                        if (usrKey-48 > int(pokemon[curPCpokemon].moves.size()) || usrKey-48<=0){ //we have to do usrKey-48 because getch() returns the ASCII value of the key pressed [0=48,1=49,...,9=57]
                            mvprintw(18, 10, "Invalid input. Please enter a number from 1 to %ld", pokemon[curPCpokemon].moves.size());
                        }else{
                            curPCmove = pokemon[curPCpokemon].moves[usrKey-48-1];      //note the move that the PC will use (-48 converts to integer from ASCII and -1 accounts for the fact that arrays index from 0)
                            break;
                        }
                    }
                    break; //WHAT IS THIS FOR?
                }else if (usrKey == '2'){ //BAG
                    int bagReturnVal = openBag(3); //open your bag and store whether or not you used a pokeball.
                    createPanel(3, 19,6, 73); //midpoint is 11, 40
                    mvprintw(4, 12, "you encountered a wild %s",wildPokemon->name.c_str());
                    //Print Player Pokemon
                    mvprintw(6, 8, "PC POKEMON: %s  ", pokemon[curPCpokemon].name.c_str());
                    mvprintw(7, 8, "LVL: %d", pokemon[curPCpokemon].level);
                    mvprintw(7, 17, "HP: %d/%d     ", pokemon[curPCpokemon].curHealth, pokemon[curPCpokemon].health);
                    mvprintw(8, 8, "ATTACK: %d    S[%d]", pokemon[curPCpokemon].attack, pokemon[curPCpokemon].special_attack);
                    mvprintw(9, 8, "DEFENSE: %d    S[%d]", pokemon[curPCpokemon].defense, pokemon[curPCpokemon].special_defense);
                    mvprintw(10,8, "SPEED: %d", pokemon[curPCpokemon].speed);
                    createPanel(11,19,6,73);            
                    //Print Opponent Pokemon
                    mvprintw(6, 40, "WILD POKEMON: %s  ", wildPokemon->name.c_str());
                    mvprintw(7, 40, "LVL: %d", wildPokemon->level);
                    mvprintw(7, 50, "HP: %d/%d     ", wildPokemon->curHealth, wildPokemon->health);
                    mvprintw(8, 40, "ATTACK: %d    S[%d]", wildPokemon->attack, wildPokemon->special_attack);
                    mvprintw(9, 40, "DEFENSE: %d    S[%d]", wildPokemon->defense, wildPokemon->special_defense);
                    mvprintw(10,40, "SPEED: %d", wildPokemon->speed);
                    if(bagReturnVal == 3){ //if user chose pokeball, they catch the pokemon (user can't even choose pokeball in bag if they have 6+ pokemon)
                        pokemon.push_back(*wildPokemon);
                        mvprintw(0,0, "YOU CAUGHT THE WILD POKEMON!");
                        return 0;
                    }
                    skipPCturn = 1;
                    break;
                }else if (usrKey == '3'){ //if the user tries to run, they have a 75% chance of success
                    
                    if (rand() % 4 != 0){
                        mvprintw(0, 0, "You successfully ran away! (pussy)");
                        return 0;
                    }else{
                        mvprintw(18, 10, "You failed to run away!");
                        getch();
                        skipPCturn = 1;
                    }
                }else if (usrKey == '4'){ //SWITCH POKEMON
                    mvprintw(12, 10, "Choose a pokemon:");
                    for(long unsigned int i=0; i<pokemon.size(); i++){
                        mvprintw(13+i, 10, "%ld. %s  HP: %d/%d", i+1, pokemon[i].name.c_str(), pokemon[i].curHealth, pokemon[i].health);
                    }
                    //get user input
                    do{
                        usrKey = getch();
                        //createPanel(11,19,6,73);
                        if (usrKey-48 > int(pokemon.size()) || usrKey-48<=0){
                            mvprintw(18, 10, "Invalid input. Please enter a number from 1 to %ld", pokemon.size());
                        }else if(pokemon[usrKey-48-1].curHealth == 0){
                            mvprintw(18, 10, "That pokemon is fainted. Please choose another pokemon.");
                        }
                    }while(usrKey-48 > int(pokemon.size()) || usrKey-48<=0 || pokemon[usrKey-48-1].curHealth == 0);
                    curPCpokemon = usrKey-48-1;
                    createPanel(11,19,6,73);
                    mvprintw(18, 10, "PC switched to %s", pokemon[curPCpokemon].name.c_str());
                    getch();
                    skipPCturn = 1;
                    break;
                }else{
                    mvprintw(18, 10, "Invalid input. Please enter a number from 1 to 4");
                }
            }

            //GEN WILD POKEMON'S ACTION
            if (wildPokemon->curHealth <= 0){ //health should never be less than 0, but just in case
                createPanel(11,19,6,73);
                mvprintw(12, 10, "You defeated the wild pokemon!");
                getch();
                return 0;
            }else{
                //gen random move
                curWildMove = wildPokemon->moves[rand() % wildPokemon->moves.size()];
            }
            
            //CARRY OUT ACTIONS
            if(!skipPCturn){
                //find who goes first
                if (10000 * curPCmove.priority + 2 * pokemon[curPCpokemon].speed + rand()%2 >= 10000*curWildMove.priority + 2*wildPokemon->speed + rand()%2){
                    
                    //PC goes first
                    attack(&pokemon[curPCpokemon], &curPCmove, wildPokemon, "PLAYER"); //have the player carry out their move

                    //NPC goes second: if the NPC is killed, then his turn is skipped, but otherwise they do their attack
                    if(wildPokemon->curHealth <= 0){
                        wildPokemon->curHealth = 0;
                        createPanel(11,19,6,73);
                        mvprintw(18, 10, "The wild %s fainted!", (wildPokemon->name).c_str());
                        getch();
                    }else{                                             
                        attack(wildPokemon, &curWildMove, &pokemon[curPCpokemon], (wildPokemon->name).c_str()); //have the NPC carry out their move
                    }
                }
                else{

                    //NPC goes first
                    attack(wildPokemon, &curWildMove, &pokemon[curPCpokemon], (wildPokemon->name).c_str()); //have the NPC carry out their move

                    //PC goes second: if the PC is killed, then his turn is skipped, but otherwise they do their attack
                    if(pokemon[curPCpokemon].curHealth <= 0){
                        pokemon[curPCpokemon].curHealth = 0;
                        createPanel(11,19,6,73);
                        mvprintw(18, 10, "PC's %s fainted!", (pokemon[curPCpokemon].name).c_str());
                        getch();
                    }else{
                        attack(&pokemon[curPCpokemon], &curPCmove, wildPokemon, "PLAYER"); //have the player carry out their move
                    }
                }
            }else {
                //only wild pokemon attacks
                attack(wildPokemon, &curWildMove, &pokemon[curPCpokemon], (wildPokemon->name).c_str()); //have the NPC carry out their move
            }

            //CHECK FOR DEATH
            //check PC death
            if (numLivePokemon()==0){
                mvprintw(0,0, "Player Lost");
                battleOver = 1; //maybe don't need this
                return 0;
            }

            //check POKEMON death
            if(wildPokemon->curHealth <= 0){
                mvprintw(0,0, "Player wins!");
                battleOver = 1; //maybe dont need this
                return 0;
            }
        }
        return 0;
    }

    //returns 1 if the player is by the water and 0 if it is not
    int playerByWater(Map* m){
        for (int i=-1; i<2; i++){
            for (int j=-1; j<2; j++){
                if (m->Tgrid[row + i][col + j] == TT_BRIDGE || m->Tgrid[row + i][col + j] == TT_WATER){
                    return 1;
                }
            }
        }
        return 0;
    }

    ~PC(){}
};

int attack(InGamePokemon* attackerPoke, Moves* attackerMove, InGamePokemon* opponentPoke, std::string attacker){
    createPanel(11,19,6,73);
    if(rand()%100< attackerMove->accuracy){ //check to see if the attack hits
        mvprintw(0,0, "%d", attackerMove->accuracy);
        //calculate pokemon damage
        int critical;
        int STAB = 1;
        //calculate STAB
        for (long unsigned int k=0; k<pokeTypesVector.size(); k++){
            if (pokeTypesVector[k].pokemon_id == attackerPoke->pokemon_id && pokeTypesVector[k].type_id == attackerMove->type_id){//check for pokemon id having a matching type as the moveset.
                STAB = 1.5;
                break;
            }
        }
        rand()%255 < attackerPoke->baseSpeed/2 ? critical = 1.5 : critical = 1; //check to see if the attack is critical.
        int damage = ((((2*(float)attackerPoke->level/5)+2)*(float)(attackerMove->power)*((float)(attackerPoke->attack)/(float)(opponentPoke->defense)))/50+2)*(float)critical*((float)(rand()%15+85)/100)*(float)STAB;
        opponentPoke->curHealth -= damage;
        //If the attack killed the pokemon, set its health to 0
        if(opponentPoke->curHealth <= 0){
            opponentPoke->curHealth = 0;
            mvprintw(18, 10, "%s chose %s. Attack defeated opponents pokemon!", attacker.c_str(), attackerMove->identifier.c_str());
        }else{
            mvprintw(18, 10, "%s chose %s. Attack landed and did %d damage!", attacker.c_str(), attackerMove->identifier.c_str(), damage);
        }
    }else{
        mvprintw(18, 10, "%s chose %s but the attack missed!", attacker.c_str(), attackerMove->identifier.c_str());
    } 
    getch();
    return 0;
}

////////////////////////////////////////////////////////////CATACOMBS//////////////////////////////////////////////////////
enum Catacomb_Tile{
    CC_WALL,        //wall
    CC_BORDER,      //surrounds the map
    CC_PATH,        //path
    CC_ENTRANCE,
    CC_cDOOR,       //closed door
    CC_oDOOR,       //open door
    CC_UNSTABLE,    //collapsable path
    CC_BUTTON       //button that opens the doors
};

//returns 0 if the player is not allowed to step on the tile and 1 if they can
int calcCatCost(int tile){
    switch (tile){
        case CC_WALL:
        case CC_BORDER:
        case CC_cDOOR:
            return 0;
        case CC_PATH:
        case CC_ENTRANCE:
        case CC_oDOOR:
        case CC_UNSTABLE:
        case CC_BUTTON:
            return 1;
    }
    return -1; //error occured
}
//class to help create a path in the dungeon
class CCpathMaker{
public:
    int identifier; //the number that this pathmaker will leave behind
    int isDone;
    int lastDir;
    int headRow;
    int headCol;
    GodQueue trail;
    int collided;
    int maxCollides;

    CCpathMaker(int startRow, int startCol, int initDir, int newIdentifier, int setMaxCollides){
        identifier = newIdentifier;
        isDone = 0;
        lastDir = initDir;
        headRow = startRow;
        headCol = startCol;
        collided = -1;
        maxCollides = setMaxCollides;
        GQinit(&trail);
    }

    //returns 0 if succesfuly moved as far as it should have, returns 1 if it hit a wall or itself.
    int move(int numTimes, int dir, int Tgrid[21][80]){
        int loopVar = 0;
        while(loopVar < numTimes && isDone == 0){
            switch(dir){
                case NORTH:
                    //check to see if you are going to hit a wall or merge with yourself if you go the given direction
                    if(headRow == 1 || Tgrid[headRow-1][headCol] == identifier || Tgrid[headRow-1][headCol-1] == identifier || Tgrid[headRow-1][headCol+1] == identifier || Tgrid[headRow-2][headCol] == identifier || Tgrid[headRow-1][headCol] == collided || Tgrid[headRow-1][headCol-1] == collided || Tgrid[headRow-1][headCol+1] == collided || Tgrid[headRow-2][headCol] == collided){
                        return 1;
                    }
                    //otherwise, if you are going to meet another path (whose identifiers are numbers larger than 25) then set isDone = 1 and return 2.
                    else if (Tgrid[headRow-1][headCol] >= 25 || Tgrid[headRow-1][headCol-1] >=25 || Tgrid[headRow-1][headCol+1] >= 25 || Tgrid[headRow-2][headCol] >=25){
                        if(maxCollides == 1){
                            isDone = 1;
                        }else{
                            if (maxCollides == 2 && collided != -1){
                                isDone = 1;
                            }else{
                                if(Tgrid[headRow-1][headCol] >= 25){collided = Tgrid[headRow-1][headCol];}
                                if(Tgrid[headRow-1][headCol-1] >=25){collided = Tgrid[headRow-1][headCol-1];}
                                if(Tgrid[headRow-1][headCol+1] >= 25){collided = Tgrid[headRow-1][headCol+1];}
                                if(Tgrid[headRow-2][headCol] >=25){collided = Tgrid[headRow-2][headCol];}
                            }
                        }
                    }
                    //otherwise, move one unit north
                    headRow--;
                    Tgrid[headRow][headCol] = identifier;
                    break;
                case EAST:
                    //check to see if you are going to hit a wall or merge with yourself if you go the given direction
                    if(headCol == 78 || Tgrid[headRow][headCol+1] == identifier || Tgrid[headRow-1][headCol+1] == identifier || Tgrid[headRow+1][headCol+1] == identifier || Tgrid[headRow][headCol+2] == identifier || Tgrid[headRow][headCol+1] == collided || Tgrid[headRow-1][headCol+1] == collided || Tgrid[headRow+1][headCol+1] == collided || Tgrid[headRow][headCol+2] == collided){
                        return 1;
                    }
                    //if you are going to meet another path (whose identifiers are numbers larger than 25) then set isDone = 1 and return 2.
                    else if (Tgrid[headRow][headCol+1] >=25 || Tgrid[headRow-1][headCol+1] >= 25 || Tgrid[headRow+1][headCol+1] >=25 || Tgrid[headRow][headCol+2] >= 25){
                        if(maxCollides == 1){
                            isDone = 1;
                        }else{
                            if (maxCollides == 2 && collided != -1){
                                isDone = 1;
                            }else{
                                if(Tgrid[headRow][headCol+1] >=25){collided = Tgrid[headRow][headCol+1];}
                                if(Tgrid[headRow-1][headCol+1] >= 25){collided = Tgrid[headRow-1][headCol+1];}
                                if(Tgrid[headRow+1][headCol+1] >=25){collided = Tgrid[headRow+1][headCol+1];}
                                if(Tgrid[headRow][headCol+2] >= 25){collided = Tgrid[headRow][headCol+2];}
                            }
                        }
                    }
                    //otherwise, move one unit east
                    headCol++;
                    Tgrid[headRow][headCol] = identifier;
                    break;
                case SOUTH:
                    //check to see if you are going to hit a wall or merge with yourself if you go the given direction
                    if(headRow == 19 || Tgrid[headRow+1][headCol] == identifier || Tgrid[headRow+1][headCol+1] == identifier || Tgrid[headRow+1][headCol-1] == identifier || Tgrid[headRow+2][headCol] == identifier || Tgrid[headRow+1][headCol] == collided || Tgrid[headRow+1][headCol+1] == collided || Tgrid[headRow+1][headCol-1] == collided || Tgrid[headRow+2][headCol] == collided){
                        return 1;
                    }
                    //if you are going to meet another path (whose identifiers are numbers larger than 25) then set isDone = 1 and return 2.
                    else if (Tgrid[headRow+1][headCol] >= 25 || Tgrid[headRow+1][headCol+1] >= 25 || Tgrid[headRow+1][headCol-1] >=25 || Tgrid[headRow+2][headCol] >= 25){
                        if(maxCollides == 1){
                            isDone = 1;
                        }else{
                            if (maxCollides == 2 && collided != -1){
                                isDone = 1;
                            }else{
                                if(Tgrid[headRow+1][headCol] >= 25){collided = Tgrid[headRow+1][headCol];}
                                if(Tgrid[headRow+1][headCol+1] >= 25){collided = Tgrid[headRow+1][headCol+1];}
                                if(Tgrid[headRow+1][headCol-1] >=25){collided = Tgrid[headRow+1][headCol-1];}
                                if(Tgrid[headRow+2][headCol] >= 25){collided = Tgrid[headRow+2][headCol];}
                            }
                        }
                    }
                    //otherwise, move one unit east
                    headRow++;
                    Tgrid[headRow][headCol] = identifier;
                    break;
                case WEST:
                    //check to see if you are going to hit a wall or merge with yourself if you go the given direction
                    if(headCol == 1 || Tgrid[headRow][headCol-1] == identifier || Tgrid[headRow-1][headCol-1] == identifier || Tgrid[headRow+1][headCol-1] == identifier || Tgrid[headRow][headCol-2] == identifier || Tgrid[headRow][headCol-1] == collided || Tgrid[headRow-1][headCol-1] == collided || Tgrid[headRow+1][headCol-1] == collided || Tgrid[headRow][headCol-2] == collided){
                        return 1;
                    }
                    //if you are going to meet another path (whose identifiers are numbers larger than 25) then set isDone = 1 and return 2.
                    else if (Tgrid[headRow][headCol-1] >= 25 || Tgrid[headRow-1][headCol-1] >= 25 || Tgrid[headRow+1][headCol-1] >=25 || Tgrid[headRow][headCol-2] >= 25){
                        if(maxCollides == 1){
                            isDone = 1;
                        }else{
                            if (maxCollides == 2 && collided != -1){
                                isDone = 1;
                            }else{
                                if(Tgrid[headRow][headCol-1] >= 25){collided = Tgrid[headRow][headCol-1];}
                                if(Tgrid[headRow-1][headCol-1] >= 25){collided = Tgrid[headRow-1][headCol-1];}
                                if(Tgrid[headRow+1][headCol-1] >=25){collided = Tgrid[headRow+1][headCol-1];}
                                if(Tgrid[headRow][headCol-2] >= 25){collided = Tgrid[headRow][headCol-2];}
                            }
                        }
                    }
                    //otherwise, move one unit West
                    headCol--;
                    Tgrid[headRow][headCol] = identifier;
                    break;
                default:
                    mvprintw(0,0,"ERROR: invalid direction [%d]in CCpathMaker's move() function", dir);refresh();
                    break;
            }
            loopVar++;
        }
        //enqueue the new position of the pathmaker (to make it a "stack" I make the node's value = size so that most recent things have the highest priority [SMART!! (:]
        GQenqueue(&trail, headRow, headCol, GQsize(&trail), NULL);
        lastDir = dir;
        return 0;
    }

    //DECONSTRUCTOR
    ~CCpathMaker(){
        //TODO
    }
};

class CatacombMap{
public:
    int Tgrid[21][80];
    int globalRow;
    int globalCol;              
    int entranceRow;       //variables to store the position of an entrance to the
    int entranceCol;       //catacombs. (-1,-1) if no entrance is on that map
    int doorN;
    int doorE;
    int doorS;
    int doorW;
    //variables to store whether the door is open or not 0=closed, 1=open
    int Nopen;
    int Sopen;
    int Eopen;
    int Wopen;
    //location of the button to open the doors
    int buttonRow;
    int buttonCol;


    CatacombMap(WorldMap* WM, int row, int col){
        //if there is no overworld map above this catacomb, create one
        if (WM->mapGrid[row*4][col*4] == NULL){
            WM->mapGrid[row*4][col*4] = new Map(WM, row*4, col*4);
            //generateCharacters(WM->mapGrid[row*4][col*4], WM, WM->player, NUMTRAINERS);
        }
        //now that there is for sure a map above the catacomb, check if there is an entrance on that map. If there is, make note of it. Else make (-1,-1) the entrance
        if( WM->mapGrid[row*4][col*4]->catacombRow > 0){
            entranceRow = WM->mapGrid[row*4][col*4]->catacombRow;
            entranceCol = WM->mapGrid[row*4][col*4]->catacombCol;
        }else{
            entranceRow = -1;
            entranceCol = -1;
        }

        //Generate North Door
        if(row == 0){                                   //you are at the top of the world so don't put a gate
            doorN=-1;
            Nopen = 0;
        }else if(WM->catacombGrid[row-1][col] != NULL){      //north map exists so take the map's southern gate column number
            doorN = WM->catacombGrid[row-1][col]->doorS;
            Nopen = WM->catacombGrid[row-1][col]->Sopen;
        }else{                                          //no north map. create random gate location
            doorN = (rand() % 78) + 1;               //generate a random number from 1 to 78 for which COL to put the north enterance
            Nopen = 0;
        }
        //Generate East Door
        if(col == 400){                                 //you are at the far right of the world so don't put a gate
            doorE =-1;
            Eopen = 0;
        }else if(WM->catacombGrid[row][col+1] != NULL){      //East map exists so take the map's west gate row number
            doorE = WM->catacombGrid[row][col+1]->doorW;
            Eopen = WM->catacombGrid[row][col+1]->Wopen;
        }else{                                          //no East map. create random gate location
            doorE = (rand() % 19) + 1;               //generate a random number from 1 to 19 for which row to put the east enterance
            Eopen = 0;
        }
        //Generate South Door
        if(row == 400){                                 //you are at the far bottom of the world so don't put a gate
            doorS=-1;
            Sopen = 0;
        }else if(WM->catacombGrid[row+1][col] != NULL){      //South map exists so take the map's northern gate column number
            doorS = WM->catacombGrid[row+1][col]->doorN;
            Sopen = WM->catacombGrid[row+1][col]->Nopen;
        }else{                                          //no South map. create random gate location
            doorS = (rand() % 78) + 1;               //generate a random number from 1 to 78 for which col to put the south enterance
            Sopen = 0;
        }
        //Generate West Door
        if(col == 0){                                   //you are at the far left of the world so don't put a gate
            doorW=-1;
            Wopen=0;
        }else if(WM->catacombGrid[row][col-1] != NULL){      //West map exists so take the map's east gate row number
            doorW = WM->catacombGrid[row][col-1]->doorE;
            Wopen = WM->catacombGrid[row][col-1]->Eopen;
        }else{                                          //no West map. create random gate location
            doorW = (rand() % 19) + 1;               //generate a random number from 1 to 19 for which row to put the West enterance
            Wopen = 0;
        }

        //make the entire map full of walls and placing border
        for (int i = 0; i<21 ; i++){
            for (int j=0; j<80; j++){
                if (i == 0 || i == 20 || j == 0 || j == 79){
                    Tgrid[i][j] = CC_BORDER;
                }else{
                    Tgrid[i][j] = CC_WALL;
                }
            }
        }
        
        //place doors & overworld entrances and clear doors
        if (doorN > 0){
            Nopen == 1 ? Tgrid[0][doorN] = CC_oDOOR : Tgrid[0][doorN] = CC_cDOOR ;
            Tgrid[1][doorN] = CC_PATH;
        }
        if (doorE > 0){
            Eopen == 1 ? Tgrid[doorE][79] = CC_oDOOR : Tgrid[doorE][79] = CC_cDOOR ;
            Tgrid[doorE][78] = CC_PATH;
        }
        if (doorS > 0){
            Sopen == 1 ? Tgrid[20][doorS] = CC_oDOOR : Tgrid[20][doorS] = CC_cDOOR ;
            Tgrid[19][doorS] = CC_PATH;
        }
        if (doorW > 0){
            Wopen == 1 ? Tgrid[doorW][0] = CC_oDOOR : Tgrid[doorW][0] = CC_cDOOR ;
            Tgrid[doorW][1] = CC_PATH;
        }
        if (entranceRow > 0){
            Tgrid[entranceRow][entranceCol] = CC_ENTRANCE;
        }

        //create pathmakers and add them to a vector
        std::vector<CCpathMaker*> paths;
        //in the following code, I put in int(paths.size())+25 as the pathmaker's ID becuase this makes each unique and above 25 AND related to the forLoop that follows
        if(doorN>0){paths.push_back(new CCpathMaker(1,doorN, SOUTH, int(paths.size())+25, 2));}     //pathmaker for northern entrance
        if(doorE>0){paths.push_back(new CCpathMaker(doorE, 78, WEST, int(paths.size())+25, 2));}    //pathmaker for eastern entrance
        if(doorS>0){paths.push_back(new CCpathMaker(19, doorS, NORTH, int(paths.size())+25, 2));}   //pathmaker for southern entrance
        if(doorW>0){paths.push_back(new CCpathMaker(doorW, 1, EAST, int(paths.size())+25, 2));}     //pathmaker for western entrance
        if(entranceRow>0){paths.push_back(new CCpathMaker(entranceRow, entranceCol, rand()%4, int(paths.size())+25, 2));} //pathmaker for overworld entrance

        int direction;
        while(pathsFinished(paths) != 1){
            for(int i=0; i< int(paths.size());i++){
                if(paths[i]->isDone == 0){ //only continue making a path if the pathmaker is not finished
                    //make sure you generate a valid direction (cant go back the way you just came, and cant travel out of bounds if you are at the boarder)
                    do{
                        direction = rand()%4;
                    }while(direction == ((paths[i]->lastDir)+2)%4); //no need to check if direction leads directly into a wall because if it does the following for loop will break
                    
                    //move the pathmaker an even number of times (2/4/6) in the given direction.
                    //The following while loop makes sure that when the pathmaker hits a wall or is about to hit itself, it changes directions and finally, moves back to its last succesful move and choses a different direction if needed
                    int numDirChanges = 0;
                    while(paths[i]->move((rand()%3+1)*2, direction, Tgrid) == 1){
                        direction = (direction + 1)%4;
                        numDirChanges++;
                        if(numDirChanges >= 4){
                            if(GQis_empty(&(paths[i]->trail)) == 1){ //if the queue has no more nodes, you cannot do any more moves so just end (doubtful this will happen)
                                paths[i]->isDone = 1;
                                break;
                            }else{
                                GQdequeue_RC(&(paths[i]->trail), &(paths[i]->headRow), &(paths[i]->headCol)); //moves the pathmakers head to the coords of its last succesful move
                                direction = rand()%4; //choose a random direction
                                numDirChanges = 0; //reset the number of direction changes count
                            }
                        }
                    }
                }
            }
        }
        //replace all the pathMaker ID trails with CC_PATH.
        for(int m=0; m<21; m++){
            for (int n=0; n<80; n++){
                if (Tgrid[m][n] >= 25){
                    //Tgrid[m][n] = Tgrid[m][n]-20;
                    Tgrid[m][n] = CC_PATH;
                }
            }
        }
        
        //GENERATE RED-HERRING TRAILS
        int randRow;
        int randCol;
        for(int i=0; i<50; i++){
            //find a random path on the map
            do{
                randRow = (rand()%9+1)*2;
                randCol = (rand()%38+1)*2;
            }while(Tgrid[randRow][randCol] != CC_PATH);

            //pick a random direction and travel until you hit a wall
            CCpathMaker randPathGen(randRow, randCol, rand()%4, CC_PATH, 1);  //give this pathmaker an identifier of CC_PATH so it won't collide with any already made paths
            
            int numDirChanges = 0;
            while(randPathGen.move(40, rand()%4, Tgrid) == 1 && randPathGen.isDone == 0){
                direction = (direction + 1)%4;
                numDirChanges++;
                if(numDirChanges >= 4){
                    if(GQis_empty(&(randPathGen.trail)) == 1){ //if the queue has no more nodes, you cannot do any more moves so just end (doubtful this will happen)
                        randPathGen.isDone = 1;
                        break;
                    }else{
                        GQdequeue_RC(&(randPathGen.trail), &(randPathGen.headRow), &(randPathGen.headCol)); //moves the pathmakers head to the coords of its last succesful move
                        direction = rand()%4; //choose a random direction
                        numDirChanges = 0; //reset the number of direction changes count
                    }
                }
            };
        }
        
        //TODO
        //for a random number of times (4 to 8)
            //pick a random spot on the map and if it is a one block boulder between 2 paths, make it a (breakable path) [only crossable once]
        
        //Place the button that opens the doors of the room on a path
        int buttonRow;
        int buttonCol;
        do{
            buttonRow = rand()%18 + 1;
            buttonCol = rand()%78 + 1;
        }while(Tgrid[buttonRow][buttonCol] != CC_PATH);
        Tgrid[buttonRow][buttonCol] = CC_BUTTON;
    }

    //finds if all the paths in the given vector have met another path and thus been completed
    int pathsFinished(std::vector<CCpathMaker*> paths){
        for (int i=0; i< int(paths.size()); i++){
            if (paths[i]->isDone == 0){
                return 0;
            }
        }
        return 1;
    }

    //function to enter a door to travel between catacombs
    void enterDoor(WorldMap* WM, PC* player){
        CatacombMap* curMap;
        if(player->row == 0){ //North Door
            player->w_row -= 4; //move player North in the world 4 overworld maps
            if(WM->catacombGrid[player->w_row/4][player->w_col/4] != NULL){    //MAP ALREADY EXISTS
                curMap = WM->catacombGrid[player->w_row/4][player->w_col/4];
                player->updateCoords(19, curMap->doorS);    //placing the PC one cell above the south gate and at the column of the south gate
            }else{  //MAP DOESN'T EXIST
                WM->catacombGrid[player->w_row/4][player->w_col/4] = new CatacombMap(WM, player->w_row/4, player->w_col/4);
                curMap = WM->catacombGrid[player->w_row/4][player->w_col/4];
                player->updateCoords(19, curMap->doorS);    //placing the PC one cell above the south gate and at the column of the south gate
            }
        }else if(player->row == 20){ //South Door
            player->w_row+= 4;
            if(WM->catacombGrid[player->w_row/4][player->w_col/4] != NULL){    //MAP ALREADY EXISTS
                curMap = WM->catacombGrid[player->w_row/4][player->w_col/4];
                player->updateCoords(1, curMap->doorN);             //placing the PC one cell below the North gate and at the column of the North gate
            }else{  //MAP DOESN'T EXIST
                WM->catacombGrid[player->w_row/4][player->w_col/4] = new CatacombMap(WM, player->w_row/4, player->w_col/4);
                curMap = WM->catacombGrid[player->w_row/4][player->w_col/4];
                player->updateCoords(1, curMap->doorN);             //placing the PC one cell below the North gate and at the column of the North gate
            }
        }else if(player->col == 0){ //West Door
            player->w_col-= 4;
            if(WM->catacombGrid[player->w_row/4][player->w_col/4] != NULL){    //MAP ALREADY EXISTS
                curMap = WM->catacombGrid[player->w_row/4][player->w_col/4];
                player->updateCoords(curMap->doorE, 78);             //placing the PC on the row of the East Gate at the column to the left of the east Gate
            }else{  //MAP DOESN'T EXIST
                WM->catacombGrid[player->w_row/4][player->w_col/4] = new CatacombMap(WM, player->w_row/4, player->w_col/4);
                curMap = WM->catacombGrid[player->w_row/4][player->w_col/4];
                player->updateCoords(curMap->doorE, 78);             //placing the PC on the row of the East Gate at the column to the left of the east Gate
            }
        }else if(player->col == 79){ //East Door
            player->w_col+= 4;
            if(WM->catacombGrid[player->w_row/4][player->w_col/4] != NULL){    //MAP ALREADY EXISTS
                curMap = WM->catacombGrid[player->w_row/4][player->w_col/4];
                player->updateCoords(curMap->doorW, 1);             //placing the PC on the row of the West Gate at the column to the right of the east Gate
            }else{  //MAP DOESN'T EXIST
                WM->catacombGrid[player->w_row/4][player->w_col/4] = new CatacombMap(WM, player->w_row/4, player->w_col/4);
                curMap = WM->catacombGrid[player->w_row/4][player->w_col/4];
                player->updateCoords(curMap->doorW, 1);             //placing the PC on the row of the West Gate at the column to the right of the east Gate
            }
        } else {
            mvprintw(0,0, "INVALID DOOR LOCATION IN 'enterDoor()' FUNCTION)");
        }

        curMap->printCatacomb(player);
        return;
    }

    //prints the full catacomb
    void printCatacomb(PC* player){
        for(int i=0; i<21; i++){
            for(int j=0; j<80; j++){
                switch(Tgrid[i][j]){
                    case CC_WALL:
                        attron(COLOR_PAIR(COLOR_CATWALL));
                        mvprintw(i+1,j,"#");
                        attroff(COLOR_PAIR(COLOR_CATWALL));
                        break;
                    case CC_BORDER:
                        mvprintw(i+1,j,"%%");
                        break;
                    case CC_PATH:
                        mvprintw(i+1,j," ");
                        break;
                    case CC_ENTRANCE:
                        mvprintw(i+1,j,"E");
                        break;
                    case CC_cDOOR:
                        attron(COLOR_PAIR(COLOR_YELLOW));
                        mvprintw(i+1,j,"@");
                        attroff(COLOR_PAIR(COLOR_YELLOW));
                        break;
                    case CC_oDOOR:
                        attron(COLOR_PAIR(COLOR_YELLOW));
                        mvprintw(i+1,j,"O");
                        attroff(COLOR_PAIR(COLOR_YELLOW));
                        break;
                    case CC_UNSTABLE:
                        mvprintw(i+1,j,"!");
                        break;
                    case CC_BUTTON:
                        attron(COLOR_PAIR(COLOR_YELLOW));
                        mvprintw(i+1,j,"*");
                        attroff(COLOR_PAIR(COLOR_YELLOW));
                        break;
                    default:
                        attron(COLOR_PAIR(COLOR_RED));
                        mvprintw(i+1,j,"%d <-ERROR", Tgrid[i][j]);
                        attroff(COLOR_PAIR(COLOR_RED));
                        break;
                }
            }
        }
        attron(COLOR_PAIR(COLOR_YELLOW));
        mvprintw(player->row + 1,player->col,"@");
        attroff(COLOR_PAIR(COLOR_YELLOW));
    }
    //prints the full catacomb
    void printPlayerCatacomb(PC* player){
        for(int i=0; i<21; i++){
            for(int j=0; j<80; j++){
                switch(Tgrid[i][j]){
                    case CC_WALL:
                        if (abs(player->row - i) + abs(player->col - j) <=5){
                            attron(COLOR_PAIR(COLOR_CATWALL));
                            mvprintw(i+1,j,"#");
                            attroff(COLOR_PAIR(COLOR_CATWALL));
                        }else{
                            attron(COLOR_PAIR(COLOR_MAGENTA));
                            mvprintw(i+1,j,".");
                            attroff(COLOR_PAIR(COLOR_MAGENTA));
                        }
                        break;
                    case CC_BORDER:
                        mvprintw(i+1,j,"%%");
                        break;
                    case CC_PATH:
                        if (abs(player->row - i) + abs(player->col - j) <=5){
                            mvprintw(i+1,j," ");
                        }else{
                            mvprintw(i+1,j,".");
                        }
                        break;
                    case CC_ENTRANCE:
                        mvprintw(i+1,j,"E");
                        break;
                    case CC_cDOOR:
                        attron(COLOR_PAIR(COLOR_RED));
                        mvprintw(i+1,j,"@");
                        attroff(COLOR_PAIR(COLOR_RED));
                        break;
                    case CC_oDOOR:
                        attron(COLOR_PAIR(COLOR_GREEN));
                        mvprintw(i+1,j,"O");
                        attroff(COLOR_PAIR(COLOR_GREEN));
                        break;
                    case CC_UNSTABLE:
                        mvprintw(i+1,j,"!");
                        break;
                    case CC_BUTTON:
                        attron(COLOR_PAIR(COLOR_YELLOW));
                        mvprintw(i+1,j,"*");
                        attroff(COLOR_PAIR(COLOR_YELLOW));
                        break;
                    default:
                        attron(COLOR_PAIR(COLOR_RED));
                        mvprintw(i+1,j,"%d <-ERROR", Tgrid[i][j]);
                        attroff(COLOR_PAIR(COLOR_RED));
                        break;
                }
            }
        }
        attron(COLOR_PAIR(COLOR_YELLOW));
        mvprintw(player->row + 1,player->col,"@");
        attroff(COLOR_PAIR(COLOR_YELLOW));
    }

};

//function to enter the catacombs
void enterCatacomb(WorldMap* WM, PC* player){
    GQdequeueAll(&(WM->charQueue));    //clear the character queue
    WM->mapGrid[player->w_row][player->w_col]->charGrid[player->row][player->col] = NULL;   //remove the player from the current map
    
    //LOADING THE CATACOMB
    if (WM->catacombGrid[player->w_row / 4][player->w_col / 4] == NULL){
        WM->catacombGrid[player->w_row/4][player->w_col/4] = new CatacombMap(WM, player->w_row/4, player->w_col/4); //create new catacomb
    }
    CatacombMap* curCatMap =WM->catacombGrid[player->w_row/4][player->w_col/4]; //store the catacomb map
    curCatMap->printPlayerCatacomb(player); refresh();
    player->inCatacombs = 1;
    int usrCatInput;
    while (player->inCatacombs == 1){
        usrCatInput = getch();
        clearScreen_top();
        switch(usrCatInput){
            case '8':
                if (calcCatCost(curCatMap->Tgrid[player->row - 1][player->col]) == 1){ player->row--;}else{ mvprintw(0,0, "Cannot move up");} break;
            case '6':
                if (calcCatCost(curCatMap->Tgrid[player->row][player->col + 1]) == 1){ player->col++;}else{mvprintw(0,0, "Cannot move right");} break;
            case '2':
                if (calcCatCost(curCatMap->Tgrid[player->row + 1][player->col]) == 1){ player->row++;}else{mvprintw(0,0, "Cannot move down"); } break;
            case '4':
                if (calcCatCost(curCatMap->Tgrid[player->row][player->col-1]) == 1){ player->col--;}else{mvprintw(0,0, "Cannot move left");}break;
            case '5':
                if (curCatMap->Tgrid[player->row][player->col] == CC_ENTRANCE){
                    player->inCatacombs = 0;
                    WM->mapGrid[player->w_row][player->w_col]->charGrid[player->row][player->col] = player;   //place the player on the new map
                    mvprintw(0,0, "BEFORE ENQUEUE"); refresh();
                    enqueueAllChars(&(WM->charQueue), WM->mapGrid[player->w_row][player->w_col]); //enqueue all characters on the new map
                    mvprintw(0,0, "AFTER ENQUEUE"); refresh();
                }else{
                    mvprintw(0,0,"No sunlight here");
                }
                break;
            case 'Q':
                player->inCatacombs = 0;
                break;
            case 'm':
                curCatMap->printCatacomb(player); refresh();
                getch();
            default:
                mvprintw(0,0,"invalid Key: %d", usrCatInput);
                break;
        }
        
        //check to see if player is now standing on an interactable square (open door, button, etc)
        if(curCatMap->Tgrid[player->row][player->col] == CC_oDOOR){
            //enter the door
            curCatMap->enterDoor(WM, player);
            curCatMap =WM->catacombGrid[player->w_row/4][player->w_col/4];
        }else if(curCatMap->Tgrid[player->row][player->col] == CC_BUTTON){
            curCatMap->Tgrid[0][curCatMap->doorN] = CC_oDOOR;
            curCatMap->Tgrid[20][curCatMap->doorS] = CC_oDOOR;
            curCatMap->Tgrid[curCatMap->doorE][79] = CC_oDOOR;
            curCatMap->Tgrid[curCatMap->doorW][0] = CC_oDOOR;
            curCatMap->Nopen = 1;
            curCatMap->Sopen = 1;
            curCatMap->Eopen = 1;
            curCatMap->Wopen = 1;
        }else if(curCatMap->Tgrid[player->row][player->col] == CC_UNSTABLE){
             //make it "caved in" so that next time you cant go through it [or turn to wall if there is no chance of opening it again]
             curCatMap->Tgrid[player->row][player->col] = CC_WALL;
        }
        curCatMap->printPlayerCatacomb(player); refresh();
    }

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//calculate the cost to move to the given cell of a map
int calc_Cost(int terrain, int chartype){
    int costs[9][15]=
    //None,Bldr,tree,path,Bridg,PMrt,Cntr,TGras,SGras,Mtn,Forst,Wtr,Gate,Grave,Catacomb
    {{MAX, MAX, MAX,  10,  10,  10,  10,  20,  10,  MAX,  MAX, MAX, 10 , 13 , 13},    //PC
     {MAX, MAX, MAX,  10,  10,  50,  50,  15,  10,  30 ,  30 , MAX, MAX, 66 , MAX},   //Hiker
     {MAX, MAX, MAX,  10,  10,  50,  50,  20,  10,  MAX,  MAX, MAX, MAX, 66 , MAX},   //Rival
     {MAX, MAX, MAX, MAX,  7,  MAX, MAX, MAX, MAX,  MAX,  MAX,   7, MAX, MAX, MAX},   //Swimmer
     {MAX, MAX, MAX,  10,  10,  50,  50,  20,  10,  MAX,  MAX, MAX, MAX, 66 , MAX},   //Pacer
     {MAX, MAX, MAX,  10,  10,  50,  50,  20,  10,  MAX,  MAX, MAX, MAX, 66 , MAX},   //Wanderer
     {MAX, MAX, MAX,  10,  10,  50,  50,  20,  10,  MAX,  MAX, MAX, MAX, 66 , MAX},   //Explorer
     {MAX, MAX, MAX,  10,  10,  50,  50,  20,  10,  MAX,  MAX, MAX, MAX, 66 , MAX},   //other
    }; 
    return costs[chartype][terrain]; 
    return 0;
}

//generate a cost map
int genCostMap(int costMap[21][80], int NPCtype, Map* terr_map, PC pc){
    int visMap[21][80];
    GQnode retNode;
    int neighborRow;
    int neighborCol;
    int newCost;
    
    //set all cells of the cost map to infinity and the vis map all to 0
    for(int i=0; i<21; i++){
        for(int j=0; j<80; j++){
            costMap[i][j] = MAX;
            visMap[i][j] = 0;
        }
    }
    costMap[pc.row][pc.col] = 0;
    //initialize my priority queue
    GodQueue q;
    GQinit(&q);
    GQenqueue(&q, pc.row, pc.col, 0, NULL);
    while (!GQis_empty(&q)){
        GQdequeue(&q, &retNode);
        
        //iterate through all neighbors of the dequeued node
        for(int m=-1; m<=1; m++){
            for(int n=-1; n<=1; n++){
                neighborRow = retNode.row + m;
                neighborCol = retNode.col + n;
                if ((m != 0 || n != 0) && neighborRow>=0 && neighborRow<=20 && neighborCol>=0 && neighborCol<=79){    //we want to ignore the cell we had just dequeued as well as any nodes out of bounds (in bounds has row indexes of 0 to 20 and column indexes of 0 to 79)
                    newCost = retNode.value + calc_Cost((terr_map->Tgrid[neighborRow][neighborCol]), NPCtype);
                    if (newCost < costMap[neighborRow][neighborCol]){
                        costMap[neighborRow][neighborCol] = newCost;
                    }
                    if (visMap[neighborRow][neighborCol] == 0){                                            //if the neighbor hasn't yet been visited before
                        GQenqueue(&q, neighborRow, neighborCol, costMap[neighborRow][neighborCol], NULL);             //add it to the queue
                        visMap[neighborRow][neighborCol] = 1;                                                   //mark it as visited
                    }
                }
            }
        }
    }
    return 0;   //success
}

int enqueueAllChars(GodQueue* GQ, Map* m){
    for(int i=0; i<21; i++){
        for(int j=0; j<80; j++){
            if(m->charGrid[i][j] != NULL){
                GQenqueue(GQ, i, j, m->charGrid[i][j]->nextCost, m->charGrid[i][j]); //enqueue each of the characters to the character queue with values equal to the cost to move to their "next row" and "next column"
            }
        }
    }
    return 0;
}

////////////////////////////////////////////////CHARACTER MOVEMENT FUNCTIONS////////////////////////////////////////////////////////////////////////

//method to fly
int move_to(WorldMap* WM, int destRow, int destCol, PC* player){
    if(destRow > 400 || destRow < 0 || destCol > 400 || destCol < 0){
        mvprintw(0,0,"ERROR: The coords (%d, %d) are out of bounds!\n", 200-destRow, destCol-200);    //the -200 and 200- to print off the UI coordinates from the array coordinates
        return 1; //ERROR OUT OF BOUNDS
    }
    Map* curMap;
    GQdequeueAll(&(WM->charQueue));    //clear the character queue
    WM->mapGrid[player->w_row][player->w_col]->charGrid[player->row][player->col] = NULL;   //remove the player from the current map
    player->w_row = destRow;
    player->w_col = destCol;


    if(WM->mapGrid[destRow][destCol] == NULL){ //if there is no map
        WM->mapGrid[destRow][destCol] = new Map(WM, destRow, destCol);
        curMap = WM->mapGrid[destRow][destCol];
        //place PC on a random path on the map
        int randRow;
        int randCol;
        do{
            randRow = (rand() % 19) + 1;
            randCol = (rand() % 79) + 1;
        }while (curMap->Tgrid[randRow][randCol] != TT_PATH || curMap->charGrid[randRow][randCol] != NULL);
        player->updateCoords(randRow, randCol);
        player->updateNextCoords(randRow, randCol); 
        curMap->charGrid[player->row][player->col] = player;   //place the player on the new map
        GQenqueue(&(WM->charQueue), player->row, player->col, player->nextCost, player); //enqueue the player to the character queue
        generateCharacters(curMap, WM, player, NUMTRAINERS);
    }else{                          //MAP ALREADY EXISTS
        curMap = WM->mapGrid[destRow][destCol];
        //place PC on a random path on the map
        int randRow;
        int randCol;
        do{
            randRow = (rand() % 19) + 1;
            randCol = (rand() % 79) + 1;
        }while (curMap->Tgrid[randRow][randCol] != TT_PATH || curMap->charGrid[randRow][randCol] != NULL);
        player->updateCoords(randRow, randCol);
        player->updateNextCoords(randRow, randCol); 
        curMap->charGrid[player->row][player->col] = player;   //place the player on the new map
        GQenqueue(&(WM->charQueue), player->row, player->col, player->nextCost, player); //enqueue the player to the character queue
        enqueueAllChars(&(WM->charQueue), curMap);
    }
    curMap->printBoard();
    
    return 0;
}

//method to move maps through a gate
int enterGate(Map* m, WorldMap* WM, PC* player){
    Map* curMap;
    GQdequeueAll(&(WM->charQueue));    //clear the character queue
    m->charGrid[player->row][player->col] = NULL;   //remove the player from the current map
    if(player->row == 0){ //North Gate
        player->w_row--; //move player North in the world
        if(WM->mapGrid[player->w_row][player->w_col] != NULL){    //MAP ALREADY EXISTS
            curMap = WM->mapGrid[player->w_row][player->w_col];
            player->updateCoords(19, curMap->gateS);    //placing the PC one cell above the south gate and at the column of the south gate
            player->updateNextCoords(19, curMap->gateS);    //updating the next coords to match the current coords
            curMap->charGrid[player->row][player->col] = player;   //place the player on the new map
            enqueueAllChars(&(WM->charQueue), curMap); //enqueue all characters on the new map
        }else{  //MAP DOESN'T EXIST
            WM->mapGrid[player->w_row][player->w_col] = new Map(WM, player->w_row, player->w_col);
            curMap = WM->mapGrid[player->w_row][player->w_col];
            player->updateCoords(19, curMap->gateS);    //placing the PC one cell above the south gate and at the column of the south gate
            player->updateNextCoords(19, curMap->gateS);    //updating the next coords to match the current coords
            curMap->charGrid[player->row][player->col] = player;   //place the player on the new map
            GQenqueue(&(WM->charQueue), player->row, player->col, player->nextCost, player); //enqueue the player to the character queue
            generateCharacters(curMap, WM, player, NUMTRAINERS);

        }
    }else if(player->row == 20){ //South Gate
        player->w_row++;
        if(WM->mapGrid[player->w_row][player->w_col] != NULL){    //MAP ALREADY EXISTS
            curMap = WM->mapGrid[player->w_row][player->w_col];
            player->updateCoords(1, curMap->gateN);             //placing the PC one cell below the North gate and at the column of the North gate
            player->updateNextCoords(1, curMap->gateN);         //updating the next coords to match the current coords
            curMap->charGrid[player->row][player->col] = player;   //place the player on the new map
            enqueueAllChars(&(WM->charQueue), curMap); //enqueue all characters on the new map
        }else{  //MAP DOESN'T EXIST
            WM->mapGrid[player->w_row][player->w_col] = new Map(WM, player->w_row, player->w_col);
            curMap = WM->mapGrid[player->w_row][player->w_col];
            player->updateCoords(1, curMap->gateN);             //placing the PC one cell below the North gate and at the column of the North gate
            player->updateNextCoords(1, curMap->gateN);         //updating the next coords to match the current coords
            curMap->charGrid[player->row][player->col] = player;   //place the player on the new map
            GQenqueue(&(WM->charQueue), player->row, player->col, player->nextCost, player); //enqueue the player to the character queue
            generateCharacters(curMap, WM, player, NUMTRAINERS);
        }
    }else if(player->col == 0){ //West Gate
        player->w_col++;
        if(WM->mapGrid[player->w_row][player->w_col] != NULL){    //MAP ALREADY EXISTS
            curMap = WM->mapGrid[player->w_row][player->w_col];
            player->updateCoords(curMap->gateE, 78);             //placing the PC on the row of the East Gate at the column to the left of the east Gate
            player->updateNextCoords(curMap->gateE, 78);         //updating the next coords to match the current coords
            curMap->charGrid[player->row][player->col] = player;   //place the player on the new map
            enqueueAllChars(&(WM->charQueue), curMap); //enqueue all characters on the new map
        }else{  //MAP DOESN'T EXIST
            WM->mapGrid[player->w_row][player->w_col] = new Map(WM, player->w_row, player->w_col);
            curMap = WM->mapGrid[player->w_row][player->w_col];
            player->updateCoords(curMap->gateE, 78);             //placing the PC on the row of the East Gate at the column to the left of the east Gate
            player->updateNextCoords(curMap->gateE, 78);         //updating the next coords to match the current coords
            curMap->charGrid[player->row][player->col] = player;   //place the player on the new map
            GQenqueue(&(WM->charQueue), player->row, player->col, player->nextCost, player); //enqueue the player to the character queue
            generateCharacters(curMap, WM, player, NUMTRAINERS);
        }
    }else if(player->col == 79){ //East Gate
        player->w_col--;
        if(WM->mapGrid[player->w_row][player->w_col] != NULL){    //MAP ALREADY EXISTS
            curMap = WM->mapGrid[player->w_row][player->w_col];
            player->updateCoords(curMap->gateW, 1);             //placing the PC on the row of the West Gate at the column to the right of the east Gate
            player->updateNextCoords(curMap->gateW, 1);         //updating the next coords to match the current coords
            curMap->charGrid[player->row][player->col] = player;   //place the player on the new map
            enqueueAllChars(&(WM->charQueue), curMap); //enqueue all characters on the new map
        }else{  //MAP DOESN'T EXIST
            WM->mapGrid[player->w_row][player->w_col] = new Map(WM, player->w_row, player->w_col);
            curMap = WM->mapGrid[player->w_row][player->w_col];
            player->updateCoords(curMap->gateW, 1);             //placing the PC on the row of the West Gate at the column to the right of the east Gate
            player->updateNextCoords(curMap->gateW, 1);         //updating the next coords to match the current coords
            curMap->charGrid[player->row][player->col] = player;   //place the player on the new map
            GQenqueue(&(WM->charQueue), player->row, player->col, player->nextCost, player); //enqueue the player to the character queue
            generateCharacters(curMap, WM, player, NUMTRAINERS);
        }
    } else {
        mvprintw(0,0, "INVALID GATE LOCATION IN 'enterGate()' FUNCTION)");
    }

    curMap->printBoard();
    return 0;

}

//takes a character object and finds the spot it will move next
int findNextPos(NPC* character, Map* m, WorldMap* WM, PC* player){
    if (character->type == CT_HIKER){
        genCostMap(WM->hiker_CM, CT_HIKER, m, *player);
        int nextVal = MAX;
        for(int i=-1; i<2; i++){
            for(int j=-1; j<2; j++){
                if (!(i == 0 && j == 0)){
                    if(WM->hiker_CM[character->row + i][character->col + j] < nextVal){
                        nextVal = WM->hiker_CM[character->row + i][character->col + j];
                        character->updateNextCoords(character->row+i, character->col+j);
                        character->nextCost = calc_Cost(m->Tgrid[character->nextRow][character->nextCol], CT_HIKER);
                    }
                }
            }
        }
        return 0;
    }
    else if (character->type == CT_RIVAL){
        genCostMap(WM->rival_CM, CT_RIVAL, m, *player);
        int nextVal = MAX;
        for(int i=-1; i<2; i++){
            for(int j=-1; j<2; j++){
                if (!(i == 0 && j == 0)){
                    if(WM->rival_CM[character->row + i][character->col + j] < nextVal){
                        nextVal = WM->rival_CM[character->row + i][character->col + j];
                        character->updateNextCoords(character->row+i, character->col+j);
                        character->nextCost = calc_Cost(m->Tgrid[character->nextRow][character->nextCol], CT_RIVAL);
                    }
                }
            }
        }
        return 0;
    }
    else if (character->type == CT_PACER){
        //CHECK COST TO MOVE IN YOUR CURRENT DIRECTION, OR OPPOSITE DIRECTION OF CURRENT DIRECTION IS NOT GOOD
        if(character->direction == NORTH){
            if(calc_Cost(m->Tgrid[character->row - 1][character->col], CT_PACER) != MAX){
                character->updateNextCoords(character->row-1, character->col);
                character->nextCost = calc_Cost(m->Tgrid[character->row - 1][character->col], CT_PACER);
                return 0;
            }
            else if(calc_Cost(m->Tgrid[character->row + 1][character->col], CT_PACER) != MAX){
                character->direction = 2;
                character->updateNextCoords(character->row+1, character->col);
                character->nextCost = calc_Cost(m->Tgrid[character->row + 1][character->col], CT_PACER);
                return 0;
            }
        }

        else if(character->direction == EAST){
            if(calc_Cost(m->Tgrid[character->row][character->col + 1], CT_PACER) != MAX){
                character->updateNextCoords(character->row, character->col+1);
                character->nextCost = calc_Cost(m->Tgrid[character->row][character->col + 1], CT_PACER);
                return 0;
            }
            else if(calc_Cost(m->Tgrid[character->row][character->col - 1], CT_PACER) != MAX){
                character->direction = 3;
                character->updateNextCoords(character->row, character->col-1);
                character->nextCost = calc_Cost(m->Tgrid[character->row][character->col - 1], CT_PACER);
                return 0;
            }
        }

        else if(character->direction == SOUTH){
            if(calc_Cost(m->Tgrid[character->row + 1][character->col], CT_PACER) != MAX){
                character->updateNextCoords(character->row+1, character->col);
                character->nextCost = calc_Cost(m->Tgrid[character->row + 1][character->col], CT_PACER);
                return 0;
            }
            else if(calc_Cost(m->Tgrid[character->row - 1][character->col], CT_PACER) != MAX){
                character->direction = 0;
                character->updateNextCoords(character->row-1, character->col);
                character->nextCost = calc_Cost(m->Tgrid[character->row - 1][character->col], CT_PACER);
                return 0;
            }
        }

        else if(character->direction == WEST){
            if(calc_Cost(m->Tgrid[character->row][character->col - 1], CT_PACER) != MAX){
                character->updateNextCoords(character->row, character->col-1);
                character->nextCost = calc_Cost(m->Tgrid[character->row][character->col - 1], CT_PACER);
                return 0;
            }
            else if(calc_Cost(m->Tgrid[character->row][character->col + 1], CT_PACER) != MAX){
                character->direction = 1;
                character->updateNextCoords(character->row, character->col+1);
                character->nextCost = calc_Cost(m->Tgrid[character->row][character->col + 1], CT_PACER);
                return 0;
            }
        }

        //IF CT_PACER CANT GO FORWARD OR BACKWARD, KEEP CURRENT SPOT
        character->updateNextCoords(character->row, character->col);
        character->nextCost = calc_Cost(m->Tgrid[character->row][character->col], CT_PACER);
        return 0;
    }
    else if (character->type == CT_WANDERER){
        int numIterations = 0;
        do{
            character->moveOneDir(m);
            numIterations++;
            if(calc_Cost(m->Tgrid[character->nextRow][character->nextCol], CT_WANDERER) == MAX || m->Tgrid[character->nextRow][character->nextCol] != m->Tgrid[character->row][character->col]){
                character->direction = rand() % 4; //choose a random direction
            }

        }while( (character->nextCost == MAX || m->Tgrid[character->nextRow][character->nextCol] != m->Tgrid[character->row][character->col]) && numIterations < 15); //the 15 is so that if by chance it is stuck in a 1x1 hole, it doesn't go into an infinite loop
        
        //if the reason that the above loop broke was because the wanderer is stuck in a 1 by 1 hole, make it stay where it is
        if (numIterations == 15){
            character->updateNextCoords(character->row, character->col);
            character->nextCost = calc_Cost(m->Tgrid[character->row][character->col], CT_WANDERER);
        }
        return 0;
    }
    else if (character->type == CT_EXPLORER){
        int numIterations = 0;
        do{
            character->moveOneDir(m);
            numIterations++;
            if(character->nextCost == MAX){
                character->direction = rand() % 4;
            }

        }while( (calc_Cost(m->Tgrid[character->nextRow][character->nextCol], CT_EXPLORER) == MAX) && numIterations < 15); //the 15 is so that if by chance it is stuck in a 1x1 hole, it doesn't go into an infinite loop
        
        //if the reason that the above loop broke was because the wanderer is stuck in a 1 by 1 hole, make it stay where it is
        if (numIterations == 5){
            character->updateNextCoords(character->row, character->col);
            character->nextCost = calc_Cost(m->Tgrid[character->row][character->col], CT_EXPLORER);
        }
        return 0;
    }
    else if (character->type == CT_SWIMMER){
        if (player->playerByWater(m)){          //if the player is by the water, make swimmer path to player
            int rowDist = player->row - character->row;
            int colDist = player->col - character->col;
            if (rowDist > 0 && calc_Cost(m->Tgrid[character->row + 1][character->col], CT_SWIMMER) != MAX){          //If player is down and swimmer can move down, move down
                character->updateNextCoords(character->row+1, character->col);
                character->nextCost = calc_Cost(m->Tgrid[character->nextRow][character->nextCol], CT_SWIMMER);
            }else if(rowDist < 0 && calc_Cost(m->Tgrid[character->row - 1][character->col], CT_SWIMMER) != MAX){     //If player is up and swimmer can move up, move up
                character->updateNextCoords(character->row-1, character->col);
                character->nextCost = calc_Cost(m->Tgrid[character->nextRow][character->nextCol], CT_SWIMMER);
            }else if(colDist > 0 && calc_Cost(m->Tgrid[character->row][character->col + 1], CT_SWIMMER) != MAX){     //If player is right and swimmer can move right, move right
                character->updateNextCoords(character->row, character->col+1);
                character->nextCost = calc_Cost(m->Tgrid[character->nextRow][character->nextCol], CT_SWIMMER);
            }else if(colDist < 0 && calc_Cost(m->Tgrid[character->row][character->col - 1], CT_SWIMMER) != MAX){      //If player is left and swimmer can move left, move left
                character->updateNextCoords(character->row, character->col-1);
                character->nextCost = calc_Cost(m->Tgrid[character->nextRow][character->nextCol], CT_SWIMMER);
            }else{                                                                                                  //else, keep swimmer where he is
                character->updateNextCoords(character->row, character->col);
                character->nextCost = calc_Cost(m->Tgrid[character->row][character->col], CT_SWIMMER);
            }
        }else{      //else randomly move the swimmer
            int numIterations = 0;
            character->direction = rand()%4;
            do{
                character->moveOneDir(m);
                numIterations++;
                if(character->nextCost == MAX){
                    character->direction = rand() % 4; //move in a random direction
                }

            }while( (calc_Cost(m->Tgrid[character->nextRow][character->nextCol], CT_SWIMMER) == MAX) && numIterations < 15); //the 15 is so that if the swimmer is stuck in a 1x1 hole, you dont go into an infinite loop
            
            //if the reason that the above loop broke was because the swimmer is stuck in a 1 by 1 hole, make it stay where it is
            if (numIterations == 15){
                character->updateNextCoords(character->row, character->col);
                character->nextCost = calc_Cost(m->Tgrid[character->row][character->col], CT_SWIMMER);
            }
        }
        return 0;
    }
    else if (character->type == CT_SENTRY){
        character->updateNextCoords(character->row, character->col);
        character->nextCost = calc_Cost(m->Tgrid[character->nextRow][character->nextCol], CT_SENTRY);
        return 0;
    }else{
        printw("INVALID CHARACTER TYPE (%d) PASSED to findNextPos() function", character->type);
        return 1; //wasn't passed a valid character type
    }
}

int generateCharacters(Map* m, WorldMap* WM, PC* player, int numTrainers){
    
    if (numTrainers < 0){
        return 1; //invalid number of trainers
    }

    int randNPC;
    //generate random trainers
    while (numTrainers > 2){
        randNPC = rand() % (CT_OTHER-1) + 1;     //generates a random number from 1 to CT_OTHER
        new NPC(randNPC, m, WM, player, player->w_row, player->w_col, player->pokemon.size());
        numTrainers--;
    }

    //make sure that there are always a hiker and rival
    switch(numTrainers){
        case 2:
            new NPC(CT_HIKER, m, WM, player, player->w_row, player->w_col, player->pokemon.size());
            new NPC(CT_RIVAL, m, WM, player, player->w_row, player->w_col, player->pokemon.size());
            break;
        case 1:
            new NPC(CT_RIVAL, m, WM, player, player->w_row, player->w_col, player->pokemon.size());
            break;
    }
    return 0;
}

//returns 0 if successful and player did NOT go. returns 1 if failed, returns 2 if player went
int moveCharacters(Map* m, WorldMap* WM, PC* player, int* retVal){
    GQnode* retNode;

    if(!(retNode = (GQnode*)calloc(sizeof(*retNode),1))){
        return 1; //couldn't make node
    }
    GQdequeue(&(WM->charQueue), retNode);

    if (retNode->character->type != CT_PLAYER){ //if dequeued character is not the player
        NPC* curNPC = dynamic_cast<NPC*>(retNode->character);   //cast it to an NPC
        
        //If NPC is already dead, just remove and ignore it (could happen when player walks into an enemy)
        if (curNPC->isDefeated == 1){
            free(retNode);
            return 0;
        }

        //if the character is an NPC and is moving into the player, battle that NPC
        if (curNPC->nextCol == player->col && curNPC->nextRow == player->row){
            //enter battle with the NPC
            player->battle(m, curNPC);
        }
        //dealing with swimmer who attack the player if they are in an adjacent cell to the player
        else if (curNPC->type == CT_SWIMMER && abs(curNPC->nextCol - player->col) <= 1 && abs(curNPC->nextRow - player->row) <=1){
            //if the swimmer is in an adjacent cell to the player, attack the player
            player->battle(m, curNPC);
        }

        //only move if there is no LIVE character already in the spot you want to go to
        else if (m->charGrid[curNPC->nextRow][curNPC->nextCol] == NULL || dynamic_cast<NPC*>(m->charGrid[curNPC->nextRow][curNPC->nextCol])->isDefeated == 1){  //I CAN DYNAMIC CAST THE CHARACTER HERE BECUASE PREVIOUSLY I CHECKED IF IT WAS THE PC AND IF IT WAS, I WOULD BATTLE IT
            //remove the character from the charMap and update node's coords
            m->charGrid[curNPC->row][curNPC->col] = NULL;
            curNPC->updateCoords(curNPC->nextRow, curNPC->nextCol);
        }else if (curNPC->type != CT_PACER){
            curNPC->direction = rand() % 4; //chose another random direction if the character is not a pacer because pacers can only go back and forth
        }

        //if the character isnt dead, update nodes next coords and next cost (this is done inside the findNextPos function). Then reenqueue the node and update the charMap
        if (curNPC->isDefeated != 1){
            findNextPos(curNPC, m, WM, player);
            GQenqueue(&(WM->charQueue), curNPC->row, curNPC->col, retNode->value + curNPC->nextCost, curNPC); //the character is enqueued with a value of its old value PLUS the new cost to move to the new terrain
            m->charGrid[curNPC->row][curNPC->col] = curNPC;
        }
    }else{ //if the ret node is the player, deal with it in main
        *retVal = retNode->value;
        free(retNode);
        return 2; //player went
        // printGQ(&(WM->charQueue)); //FOR TESTING REMOVETHIS
    }
    free(retNode);
    return 0;
}

///////////////////////////////////////////////////////////////////UI FUNCTIONS//////////////////////////////////////////////////////////////////////////////////

int displayTrainerList(WorldMap* WM, Map* m, PC* player){
    clearScreen_top();
    createPanel(3, 19, 25, 55);
    mvprintw(4,35, "TRAINER LIST");
    int numTrainers = 0;    //keep track of how many trainers have been put in the array (and what index the next trainer should be input at)
    
    //first count the number of trainers on the map
    for(int i=0; i<21; i++){
        for(int j=0; j<80; j++){
            if(m->charGrid[i][j] != NULL && m->charGrid[i][j]->type != CT_PLAYER){
                numTrainers++;
            }
        }
    }

    //create an array of pointers to trainers and add all the trainers on the map to that array
    Character* trainers[numTrainers];
    int trainersAdded = 0;
    for(int i=0; i<21; i++){
        for(int j=0; j<80; j++){
            if(m->charGrid[i][j] != NULL && m->charGrid[i][j]->type != CT_PLAYER){
                trainers[trainersAdded] = m->charGrid[i][j];
                trainersAdded++;
            }
        }
    }

    int startIndex = 0;
    int usrKey;
    int Xdist, Ydist;
    Character currentCharacter;
    do{
        for (int i=0; i < 11; i++){  //only print 11 characters to the screen at a time
            if (i+startIndex >= numTrainers){   //if you reach the end of your trainer array, stop printing
                break;
            }
            currentCharacter = *trainers[startIndex + i];

            //calculate distance from player to trainer
            Xdist = currentCharacter.col - player->col;
            Ydist = currentCharacter.row - player->row;
            
            //PRINT STUFF
            //print character type
            mvprintw(i+6,30,"%c ", CT_to_Char(currentCharacter.type));

            //print North/South info
            if (Ydist > 0){
                printw("%d South, ", abs(Ydist));
            }else{
                printw("%d North, ", abs(Ydist));
            }
            //print East/West info
            if (Xdist > 0){
                printw("%d East", abs(Xdist));
            }else{
                printw("%d West", abs(Xdist));
            }
        }

        usrKey = getch();
        if (usrKey == KEY_DOWN && startIndex + 11 < numTrainers){ 
            createPanel(3, 19, 25, 55);
            mvprintw(4,35, "TRAINER LIST");
            startIndex += 11;
        } else if (usrKey == KEY_UP && startIndex > 0){
            createPanel(3, 19, 25, 55);
            mvprintw(4,35, "TRAINER LIST");
            startIndex -= 11;
        }
    }while(usrKey != KEY_ESC);
    clearScreen();
    clearScreen_top();
    return 0;
}

int enterBuilding(Map* m, PC* player, int buildingType){
    int usrKey;
    clearScreen_top();
    createPanel(3, 19, 10, 69);
    if (buildingType == TT_PCENTER){
        mvprintw(4,15, "welcome to the Pokecenter!");
        mvprintw(6,15, "your pokemon have been healed");
        mvprintw(7,15, "press '<' to leave");

        //HEAL ALL PLAYER POKEMON
        for (long unsigned int i=0; i<player->pokemon.size(); i++){
            player->pokemon[i].curHealth = player->pokemon[i].health;
        }
        do{
            usrKey = getch();
        }while(usrKey != KEY_LEFT);
        clearScreen_top();
        mvprintw(0,0, "you left the pokecenter");
        return 0;
    }else if(buildingType == TT_PMART){
        mvprintw(4,15, "welcome to the PokeMart!");
        mvprintw(5,15, "your items have been refilled");
        mvprintw(6,15, "press '<' to leave");
        
        //REFILL ITEMS
        player->numPokeballs = 5;
        player->numPotions = 10;
        player->numRevives = 3;
        
        do{
            usrKey = getch();
        }while(usrKey != KEY_LEFT);
        clearScreen_top();
        mvprintw(0,0, "you left the PokeMart");
        return 0;
    }else{
        clearScreen_top();
        mvprintw(0,0, "INVALID BUILDING TYPE");
        return 1; //invalid building type
    }
}

//////////////////////////////////////////////////////////////////////MAIN/////////////////////////////////////////////////////////////////////////

int main(int argc, char *argv[]){
    srand(time(NULL));
    int gameOver = 0;       //keep track of when the game should end
    int key;
    int skipLastTurn = 0;
    int retVal = 0;
    Map* curMap;


    //initialize the number of trainers
    if (argc == 1){
        NUMTRAINERS = 10;
    }else if(argc == 2){
        NUMTRAINERS = atoi(argv[1]);
    }else{
        printf("ERROR: Too many arguments in command line.");
    }

    //for ncurses
    initTerminal();

    //create the first map
    WorldMap WM;
    WM.mapGrid[200][200] = new Map(&WM, 200, 200);

    //initialize the player
    PC player(&WM,*WM.mapGrid[200][200], 200, 200);
    mvprintw(0,0, "you chose %s", player.pokemon[0].name.c_str());

    GQenqueue(&(WM.charQueue), player.row, player.col, 0, &player);
    WM.mapGrid[player.w_row][player.w_col]->charGrid[player.row][player.col] = &player; //putting the player character into the current map

    ////////////////////////////////////////////////////////////////
    generateCharacters(WM.mapGrid[player.w_row][player.w_col], &WM, &player, NUMTRAINERS);
    //print board
    WM.mapGrid[player.w_row][player.w_col]->printBoard();

    while (!gameOver){
        curMap = WM.mapGrid[player.w_row][player.w_col];
        key = getch();
        skipLastTurn = 0;
        switch(key){
            case '7':
            case 'y':
                //attempt to move PC one cell to the upper left (should check for capability in the move characters function)
                if (calc_Cost(curMap->Tgrid[player.row -1][player.col - 1], CT_PLAYER) != MAX){
                    clearScreen_top();
                    mvprintw(0,0, "move up left");
                    player.updateNextCoords(player.row-1, player.col-1);
                }
                else{
                    clearScreen_top();
                    mvprintw(0,0, "can't move up left");
                }
                break;
            case '8':
            case 'k':
                //attempt to move PC one cell up (should check for capability in the move characters function)
                if(calc_Cost(curMap->Tgrid[player.row -1][player.col], CT_PLAYER) != MAX){
                    clearScreen_top();
                    mvprintw(0,0,"move up");
                    player.updateNextCoords(player.row-1, player.col);
                }
                else{
                    clearScreen_top();
                    mvprintw(0,0,"can't move up");
                }
                break;
            case '9':
            case 'u':
                //attempt to move PC one cell to the upper right (should check for capability in the move characters function)
                if(calc_Cost(curMap->Tgrid[player.row -1][player.col + 1], CT_PLAYER) != MAX){
                    clearScreen_top();
                    mvprintw(0,0,"move up right");
                    player.updateNextCoords(player.row-1, player.col+1);
                }
                else{
                    clearScreen_top();
                    mvprintw(0,0,"can't move up right");
                }
                break;
            case '6':
            case 'l':
                //attempt to move PC one cell to the right (should check for capability in the move characters function)
                if(calc_Cost(curMap->Tgrid[player.row][player.col + 1], CT_PLAYER) != MAX){
                    clearScreen_top();
                    mvprintw(0,0,"move right");
                    player.updateNextCoords(player.row, player.col+1);
                }
                else{
                    clearScreen_top();
                    mvprintw(0,0,"can't move right");
                }
                break;
            case '3':
            case 'n':
                //attempt to move PC one cell to the lower right (should check for capability in the move characters function)
                if(calc_Cost(curMap->Tgrid[player.row + 1][player.col + 1], CT_PLAYER) != MAX){
                    clearScreen_top();
                    mvprintw(0,0,"move down right");
                    player.updateNextCoords(player.row+1, player.col+1);
                }
                else{
                    clearScreen_top();
                    mvprintw(0,0,"can't move down right");
                }
                break;
            case '2':
            case 'j':
                //attempt to move PC one cell down (should check for capability in the move characters function)
                if(calc_Cost(curMap->Tgrid[player.row + 1][player.col], CT_PLAYER) != MAX){
                    clearScreen_top();
                    mvprintw(0,0,"move down");
                    player.updateNextCoords(player.row+1, player.col);
                }
                else{
                    clearScreen_top();
                    mvprintw(0,0,"can't move down");
                }
                break;
            case '1':
            case 'b':
                //attempt to move PC one cell to the lower left
                if(calc_Cost(curMap->Tgrid[player.row + 1][player.col - 1], CT_PLAYER) != MAX){
                    clearScreen_top();
                    mvprintw(0,0,"move down left");
                    player.updateNextCoords(player.row+1, player.col-1);
                }
                else{
                    clearScreen_top();
                    mvprintw(0,0,"can't move down left");
                }
                break;
            case '4':
            case 'h':
                //attempt to move PC one cell to the left
                if(calc_Cost(curMap->Tgrid[player.row][player.col - 1], CT_PLAYER) != MAX){
                    clearScreen_top();
                    mvprintw(0,0,"move left");
                    player.updateNextCoords(player.row, player.col-1);
                }
                else{
                    clearScreen_top();
                    mvprintw(0,0,"can't move left");
                }
                break;
            case KEY_RIGHT:
                //attempt to enter building
                if(curMap->Tgrid[player.row][player.col] == TT_PMART){
                    clearScreen_top();
                    mvprintw(0,0, "entering pokemart");
                    enterBuilding(curMap, &player, TT_PMART);
                }
                else if(curMap->Tgrid[player.row][player.col] == TT_PCENTER){
                    clearScreen_top();
                    mvprintw(0,0, "entering pokecenter");
                    enterBuilding(curMap, &player, TT_PCENTER);
                }
                else{
                    clearScreen_top();
                    mvprintw(0,0, "no building to enter");
                }
                break;
            case '5':
            case ' ':
            case '.':
                //rest for a turn. NPCs still move
                clearScreen_top();
                mvprintw(0,0, "rest");
                break;
            case 't':
                //display a list of trainers on the map, with their symbol and position relative to the PC (e.g.: "r, 2 north and 14 west").
                displayTrainerList(&WM, curMap, &player);
                skipLastTurn = 1;
                break;
            case 'Q':
                gameOver = 1;
                skipLastTurn = 1;
                break;
            case 'f':
                clearScreen_top();
                mvprintw(0,0, "Enter a map to fly to [world_row, world_col]: "); refresh();
                int destRow; 
                int destCol;
                scanf("%d %d", &destRow, &destCol);
                if(move_to(&WM, 200-destRow, destCol+200, &player) != 1){   //if you succesfuly fly to a location, print that location. Else the function already prints an error. The offsets of 200 are to make (0,0) the center of the world
                    clearScreen_top();
                    mvprintw(0,0, "Flying to (%d, %d)", destRow, destCol);
                    curMap = WM.mapGrid[player.w_row][player.w_col];
                }
                curMap->printBoard();
                refresh();
                break;
            case 'L':
                //level up your pokemon
                player.pokemon[0].levelUp();
                createPanel(4, 18, 10, 69);
                mvprintw(5, 13, "YOU LEVELED UP YOUR POKEMON! (press ESC to leave)");
                mvprintw(6, 13, "NAME: %s", player.pokemon[0].name.c_str());
                mvprintw(7, 13, "LEVEL: %d", player.pokemon[0].level);
                mvprintw(8, 13, "HP: %d", player.pokemon[0].health);
                mvprintw(9, 13, "ATTACK: %d", player.pokemon[0].attack);
                mvprintw(10, 13, "DEFENSE: %d", player.pokemon[0].defense);
                mvprintw(11, 13, "SPECIAL ATTACK: %d", player.pokemon[0].special_attack);
                mvprintw(12, 13, "SPECIAL DEFENSE: %d", player.pokemon[0].special_defense);
                mvprintw(13, 13, "SPEED: %d", player.pokemon[0].speed);
                mvprintw(14, 13, "SHINY: %d", player.pokemon[0].isShiny);
                mvprintw(15, 13, "MOVE 1: %d", player.pokemon[0].moves[0].id);
                mvprintw(16, 13, "MOVE 2: %d", player.pokemon[0].moves[1].id);
                while (getch() != KEY_ESC){};//wait for escape to be pressed
                skipLastTurn =1;
                break;
            case 'B': //access bag
                player.openBag(1); //opening the bag in the overworld (so "location" argument is 1)
                break;
            default:
                //print out key that was pressed
                clearScreen_top();
                mvprintw(0,0,"Unknown Key: %d", key);
                skipLastTurn = 1;
        }

        //Dealing with PC movement
        if(curMap->charGrid[player.nextRow][player.nextCol] != NULL && curMap->charGrid[player.nextRow][player.nextCol]->type != CT_PLAYER && dynamic_cast<NPC*>(curMap->charGrid[player.nextRow][player.nextCol])->isDefeated == 0){ //I CAN DYNAMIC CAST THE CHARACTER HERE BECUASE EARLIER IN THE CONDITIONAL I CHECKED IF IT WAS THE PC
            player.battle(curMap, dynamic_cast<NPC*>(curMap->charGrid[player.nextRow][player.nextCol]));
        }
        //only move if there is no LIVE character already in the spot you want to go to
        else {
            //remove the character from the charMap and update node's coords
            curMap->charGrid[player.row][player.col] = NULL;
            player.updateCoords(player.nextRow, player.nextCol);
            
            //checking to see if the PC stepped on a catacomb entrance
            if(curMap->Tgrid[player.row][player.col] == TT_CATACOMB){
                enterCatacomb(&WM, &player);
                retVal=0;
                curMap = WM.mapGrid[player.w_row][player.w_col]; //update the current map to be the new one the player has emeged to.
            }
        }

        //if player stepped onto tall grass, see if there is a pokemon [10% chance]
        if(curMap->Tgrid[player.row][player.col] == TT_TGRASS){
            if(rand()%10 == 7){
                InGamePokemon wildPokemon = InGamePokemon(player.w_row, player.w_col);
                player.battleWildPokemon(&wildPokemon);
            }
        }

        GQenqueue(&(WM.charQueue), player.row, player.col, retVal + player.nextCost, &player); //the character is enqueued with a value of its old value PLUS the new cost to move to the new terrain
        curMap->charGrid[player.row][player.col] = &player;
        curMap->printBoard();
        if(curMap->Tgrid[player.row][player.col] == TT_GATE){
            enterGate(curMap, &WM, &player);
            curMap = WM.mapGrid[player.w_row][player.w_col];
        }

        curMap->printBoard();
        //loop to make sure the other NPC's move until its the players turn again (nothing happens when its not the players turn)
        while(skipLastTurn == 0 && moveCharacters(curMap, &WM, &player, &retVal) != 2){ //the move characters function returns 0 if an NPC is dequeued and a -1 if it fails
            //curMap->printBoard();
            mvprintw(25,0, "%d", rand()%100); refresh(); //FORTESTING
        }
        curMap->printBoard();
    }

    endwin();
    return 0;
}