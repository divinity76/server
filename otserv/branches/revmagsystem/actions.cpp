//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
// 
//////////////////////////////////////////////////////////////////////
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//////////////////////////////////////////////////////////////////////


#include "definitions.h"
#include "const74.h"
#include "player.h"
#include "monster.h"
#include "npc.h"
#include "game.h"
#include "item.h"
#include "spells.h"
#include "condition.h"

#include <libxml/xmlmemory.h>
#include <libxml/parser.h> 

#include "actions.h"

extern Spells spells;

bool readXMLInteger(xmlNodePtr p, const char *tag, int &value);

Actions::Actions(Game* igame)
:game(igame)
{
	//                   
}

Actions::~Actions()
{
	ActionUseMap::iterator it = useItemMap.begin();
	while(it != useItemMap.end()) {
		delete it->second;
		useItemMap.erase(it);
		it = useItemMap.begin();
	}
}

bool Actions::reload(){
	this->loaded = false;
	//unload
	ActionUseMap::iterator it = useItemMap.begin();
	while(it != useItemMap.end()) {
		delete it->second;
		useItemMap.erase(it);
		it = useItemMap.begin();
	}
	it = uniqueItemMap.begin();
	while(it != uniqueItemMap.end()) {
		delete it->second;
		uniqueItemMap.erase(it);
		it = uniqueItemMap.begin();
	}
	//load
	return loadFromXml(datadir);
}

bool Actions::loadFromXml(const std::string &_datadir)
{
	this->loaded = false;
	Action *action = NULL;
	
	datadir = _datadir;
	
	std::string filename = datadir + "actions/actions.xml";
	std::transform(filename.begin(), filename.end(), filename.begin(), tolower);
	xmlDocPtr doc = xmlParseFile(filename.c_str());

	if (doc){
		this->loaded=true;
		xmlNodePtr root, p;
		root = xmlDocGetRootElement(doc);
		
		if (xmlStrcmp(root->name,(const xmlChar*) "actions")){
			xmlFreeDoc(doc);
			return false;
		}
		p = root->children;
        
		while (p)
		{
			const char* str = (char*)p->name;
			
			if (strcmp(str, "action") == 0){
				int itemid,uniqueid,actionid;
				if(readXMLInteger(p,"itemid",itemid)){
					action = loadAction(p);
					useItemMap[itemid] = action;
					action = NULL;
				}
				else if(readXMLInteger(p,"uniqueid",uniqueid)){
					action = loadAction(p);
					uniqueItemMap[uniqueid] = action;
					action = NULL;
				}
				else if(readXMLInteger(p,"actionid",actionid)){
					action = loadAction(p);
					actionItemMap[actionid] = action;
					action = NULL;
				}
				else{
					std::cout << "missing action id." << std::endl;
				}
			}
			p = p->next;
		}
		
		xmlFreeDoc(doc);
	}
	return this->loaded;
}

Action *Actions::loadAction(xmlNodePtr xmlaction){
	Action *action = NULL;
	const char* scriptfile = (const char*)xmlGetProp(xmlaction,(xmlChar*)"script");
	if(scriptfile){
		action = new Action(game,datadir, datadir + std::string("actions/scripts/") + scriptfile);
		if(action->isLoaded()){
			const char* sallow = (const char*)xmlGetProp(xmlaction,(xmlChar*)"allowfaruse");
			if(sallow && strcmp(sallow,"1")==0){
				action->setAllowFarUse(true);
			}
		}
		else{
			delete action;
			action = NULL;
		}
	}
	else{
		std::cout << "Missing script tag."  << std::endl;
	}
	return action;
}

int Actions::canUse(const Player *player,const Position &pos) const
{
	if(pos.x != 0xFFFF){
		int dist_x = std::abs(pos.x - player->pos.x);
		int dist_y = std::abs(pos.y - player->pos.y);
		if(dist_x > 1 || dist_y > 1 || (pos.z != player->pos.z)){
			return 1;
		}
	}
	return 0;
}

Action *Actions::getAction(const Item *item){
	if(item->getUniqueId() != 0){
		ActionUseMap::iterator it = uniqueItemMap.find(item->getUniqueId());
    	if (it != uniqueItemMap.end()){
			return it->second;
		}
	}
	if(item->getActionId() != 0){
		ActionUseMap::iterator it = actionItemMap.find(item->getActionId());
    	if (it != actionItemMap.end()){
	    	return it->second;
		}
	}
	ActionUseMap::iterator it = useItemMap.find(item->getID());
    if (it != useItemMap.end()){
	   	return it->second;
	}
	
	return NULL;
}

void Actions::UseItem(Player* player, const Position &pos,const unsigned char stack, 
	const unsigned short itemid, const unsigned char index)
{	
	if(canUse(player,pos)== 1){
		player->sendCancel("Too far away.");
		return;
	}
	Item *item = dynamic_cast<Item*>(game->getThing(pos,stack,player));
	if(!item){
		std::cout << "no item" << std::endl;
		player->sendCancel("You can not use this object.");
		return;
	}
	
	if(item->getID() != itemid){
		std::cout << "no id" << std::endl;
		player->sendCancel("You can not use this object.");
		return;
	}
	
	//look for the item in action maps	
	Action *action = getAction(item);
	
	//if found execute it
	if(action){
		PositionEx posEx(pos,stack);
		if(action->executeUse(player,item,posEx,posEx)){
			return;
		}
	}
	
	//if it is a container try to open it
	if(dynamic_cast<Container*>(item)){
		if(openContainer(player,dynamic_cast<Container*>(item),index))
			return;
	}

	//we dont know what to do with this item
	player->sendCancel("You can not use this object.");
	return;
}

bool Actions::openContainer(Player *player,Container *container, const unsigned char index){
	if(container->depot == 0){ //normal container
		unsigned char oldcontainerid = player->getContainerID(container);
		if(oldcontainerid != 0xFF) {
			player->closeContainer(oldcontainerid);
			player->sendCloseContainer(oldcontainerid);
		}
		else {
			player->sendContainer(index, container);
		}
	}
	else{// depot container
		Container *container2 = player->getDepot(container->depot);
		if(container2){
			//update depot coordinates					
			container2->pos = container->pos;
			player->sendContainer(index, container2);
		}
		else{
			return false;
		}
	}
	return true;
}

void Actions::UseItemEx(Player* player, const Position &from_pos,
	const unsigned char from_stack,const Position &to_pos,
	const unsigned char to_stack,const unsigned short itemid)
{
	if(canUse(player,from_pos) == 1){
		player->sendCancel("Too far away.");
		return;
	}
	
	Item *item = dynamic_cast<Item*>(game->getThing(from_pos,from_stack,player));
	if(!item)
		return;
	
	if(item->getID() != itemid) {
		player->sendCancel("Sorry not possible.");
		return;
	}
	
	Action *action = getAction(item);
	
	if(action){
		if(action->allowFarUse() == false){
			if(canUse(player,to_pos) == 1){
				player->sendCancel("Too far away.");
				return;
			}
		}
		PositionEx posFromEx(from_pos,from_stack);
		PositionEx posToEx(to_pos,to_stack);    	
    	if(action->executeUse(player,item,posFromEx,posToEx))
    		return;
	}
    
	//Runes
	std::map<unsigned short, Spell*>::iterator sit = spells.getAllRuneSpells()->find(item->getID());
	if(sit != spells.getAllRuneSpells()->end()) {
		std::string var = std::string("");
		bool success = sit->second->castSpell(player, to_pos, var);

		if(success) {
			item->setItemCharge(std::max((int)item->getItemCharge() - 1, 0) );
			if(item->getItemCharge() == 0) {
				game->removeThing(player, from_pos, item);
			}
		}
		
		return;
	}

	//not found
	player->sendCancel("You can not use this object.");
}

void Actions::UseItemEx(Player* player, const Position &from_pos,
	const unsigned char from_stack, unsigned short itemid, Creature* creature)
{
	if(canUse(player,from_pos) == 1){
		player->sendCancel("Too far away.");
		return;
	}

	Item *item = dynamic_cast<Item*>(game->getThing(from_pos,from_stack,player));
	if(!item)
		return;
	
	if(item->getID() != itemid) {
		player->sendCancel("Sorry not possible.");
		return;
	}

	//Runes
	std::map<unsigned short, Spell*>::iterator sit = spells.getAllRuneSpells()->find(item->getID());
	if(sit != spells.getAllRuneSpells()->end()) {
		std::string var = std::string("");
		bool success = sit->second->castSpell(player, creature);

		if(success) {
			item->setItemCharge(std::max((int)item->getItemCharge() - 1, 0) );
			if(item->getItemCharge() == 0) {
				game->removeThing(player, from_pos, item);
			}
		}
		
		return;
	}

	//not found
	player->sendCancel("You can not use this object.");
}

//

bool readXMLInteger(xmlNodePtr p, const char *tag, int &value)
{
	const char* sinteger = (const char*)xmlGetProp(p, (xmlChar*)tag);
	if(!sinteger)
		return false;
	else{
		unsigned short integer = atoi(sinteger);
		value = integer;
		return true;
	}
}


Action::Action(Game* igame,const std::string &datadir, const std::string &scriptname)
{
	loaded = false;
	allowfaruse = false;
	script = new ActionScript(igame,datadir,scriptname);
	if(script->isLoaded())
		loaded = true;
}

Action::~Action()
{
	if(script) {
		delete script;
		script = NULL;
	}
}


bool Action::executeUse(Player *player,Item* item, PositionEx &posFrom, PositionEx &posTo)
{
	//onUse(uidplayer, item1,position1,item2,position2)
	script->ClearMap();
	script->_player = player;
	PositionEx playerpos = player->pos;
	unsigned int cid = script->AddThingToMap((Thing*)player,playerpos);
	unsigned int itemid1 = script->AddThingToMap(item,posFrom);
	lua_State*  luaState = script->getLuaState();
	
	lua_pushstring(luaState, "onUse");
	lua_gettable(luaState, LUA_GLOBALSINDEX);
	
	lua_pushnumber(luaState, cid);
	script->internalAddThing(luaState,item,itemid1);
	script->internalAddPositionEx(luaState,posFrom);
	//std::cout << "posTo" <<  (Position)posTo << " stack" << (int)posTo.stackpos <<std::endl;
	Thing *thing = script->game->getThing((Position)posTo,posTo.stackpos,player);
	if(thing && posFrom != posTo){
		int thingId2 = script->AddThingToMap(thing,posTo);
		script->internalAddThing(luaState,thing,thingId2);
		script->internalAddPositionEx(luaState,posTo);
	}
	else{
		script->internalAddThing(luaState,NULL,0);
		PositionEx posEx;
		script->internalAddPositionEx(luaState,posEx);
	}
	
	lua_pcall(luaState, 5, 1, 0);
	
	bool ret = (bool)script->internalGetNumber(luaState);
	
	return ret;
}


std::map<unsigned int,KnownThing*> BaseScript::uniqueIdMap;

BaseScript::BaseScript(Game *igame):
game(igame), _player(NULL)
{
	lastuid = 0;
	this->loaded = false;

	luaState = lua_open();
	luaopen_loadlib(luaState);
	luaopen_base(luaState);
	luaopen_math(luaState);
	luaopen_string(luaState);
	luaopen_io(luaState);

	setGlobalNumber("addressOfBaseScript", (int)this);
	//registerFunctions();
}

bool BaseScript::loadScript(const std::string& scriptname)
{
	if(scriptname == "")
		return false;

	FILE* in=fopen(scriptname.c_str(), "r");
	if(!in){
		std::cout << "Error: Can not open " << scriptname.c_str() << std::endl;
		return false;
	}
	else
		fclose(in);

	registerFunctions();
	lua_dofile(luaState, scriptname.c_str());
	this->loaded = true;
	return true;
}


void BaseScript::ClearMap()
{
	std::map<unsigned int,KnownThing*>::iterator it;
	for(it = ThingMap.begin(); it != ThingMap.end();it++ ){
		delete it->second;
		it->second = NULL;
	}
	ThingMap.clear();
	lastuid = 0;
}

void BaseScript::AddThingToMapUnique(Thing *thing){
	Item *item = dynamic_cast<Item*>(thing);
	if(item && item->getUniqueId() != 0 ){
		unsigned short uid = item->getUniqueId();
		KnownThing *tmp = uniqueIdMap[uid];
		if(!tmp){
			KnownThing *tmp = new KnownThing;
			tmp->thing = thing;
			tmp->type = thingTypeItem;
			uniqueIdMap[uid] = tmp;
		}
		else{
			std::cout << "Duplicate uniqueId " <<  uid << std::endl;
		}
	}
}

void BaseScript::UpdateThingPos(int uid, PositionEx &pos){
	KnownThing *tmp = ThingMap[uid];
	if(tmp){
		tmp->pos = pos;
	}		
}

unsigned int BaseScript::AddThingToMap(Thing *thing,PositionEx &pos)
{
	Item *item = dynamic_cast<Item*>(thing);
	if(item && item->pos.x != 0xFFFF && item->getUniqueId()){
		unsigned short uid = item->getUniqueId();
		KnownThing *tmp = uniqueIdMap[uid];
		if(!tmp){
			std::cout << "Item with unique id not included in the map!." << std::endl;
		}
		KnownThing *newKT = new KnownThing;
		newKT->thing = thing;
		newKT->type = thingTypeItem;
		newKT->pos = pos;
		ThingMap[uid] = newKT;
		return uid;
	}
	
	std::map<unsigned int,KnownThing*>::iterator it;
	for(it = ThingMap.begin(); it != ThingMap.end();it++ ){
		if(it->second->thing == thing){
			return it->first;
		}
	}
	
	KnownThing *tmp = new KnownThing;
	tmp->thing = thing;
	tmp->pos = pos;
	
	if(dynamic_cast<Item*>(thing))
		tmp->type = thingTypeItem;
	else if(dynamic_cast<Player*>(thing))
		tmp->type = thingTypePlayer;
	else if(dynamic_cast<Monster*>(thing))
		tmp->type = thingTypeMonster;
	else if(dynamic_cast<Npc*>(thing))
		tmp->type = thingTypeNpc;	
	else
		tmp->type = thingTypeUnknown;
	
	lastuid++;
	while(ThingMap[lastuid]){
		lastuid++;
	}
	ThingMap[lastuid] = tmp;
	return lastuid;
}

const KnownThing* BaseScript::GetThingByUID(int uid)
{
	KnownThing *tmp = ThingMap[uid];
	if(tmp)
		return tmp;
	tmp = uniqueIdMap[uid];
	if(tmp && tmp->thing->pos.x != 0xFFFF){
		KnownThing *newKT = new KnownThing;
		newKT->thing = tmp->thing;
		newKT->type = tmp->type;
		newKT->pos = tmp->thing->pos;
		ThingMap[uid] = newKT;
		return newKT;
	}
	return NULL;
}

const KnownThing* BaseScript::GetItemByUID(int uid)
{
	const KnownThing *tmp = GetThingByUID(uid);
	if(tmp){
		if(tmp->type == thingTypeItem)
			return tmp;
	}
	return NULL;
}

const KnownThing* BaseScript::GetCreatureByUID(int uid)
{
	const KnownThing *tmp = GetThingByUID(uid);
	if(tmp){
		if(tmp->type == thingTypePlayer || tmp->type == thingTypeMonster
			|| tmp->type == thingTypeNpc )
			return tmp;
	}
	return NULL;
}

const KnownThing* BaseScript::GetPlayerByUID(int uid)
{
	const KnownThing *tmp = GetThingByUID(uid);
	if(tmp){
		if(tmp->type == thingTypePlayer)
			return tmp;
	}
	return NULL;
}


int BaseScript::registerFunctions()
{
	//getPlayerSpeed(uid)
	lua_register(luaState, "getPlayerSpeed", BaseScript::luaActionGetPlayerSpeed);
	//getPlayerFood(uid)
	lua_register(luaState, "getPlayerFood", BaseScript::luaActionGetPlayerFood);
	//getPlayerHealth(uid)	
	lua_register(luaState, "getPlayerHealth", BaseScript::luaActionGetPlayerHealth);
	//getPlayerMana(uid)
	lua_register(luaState, "getPlayerMana", BaseScript::luaActionGetPlayerMana);
	//getPlayerLevel(uid)
	lua_register(luaState, "getPlayerLevel", BaseScript::luaActionGetPlayerLevel);
	//getPlayerMagLevel(uid)
	lua_register(luaState, "getPlayerMagLevel", BaseScript::luaActionGetPlayerMagLevel);
	//getPlayerName(uid)	
	lua_register(luaState, "getPlayerName", BaseScript::luaActionGetPlayerName);
	//getPlayerAccess(uid)	
	lua_register(luaState, "getPlayerAccess", BaseScript::luaActionGetPlayerAccess);
	//getPlayerPosition(uid)
	lua_register(luaState, "getPlayerPosition", BaseScript::luaActionGetPlayerPosition);
	//getPlayerSkill(uid,skillid)
	lua_register(luaState, "getPlayerSkill", BaseScript::luaActionGetPlayerSkill);
	//getPlayerStorageValue(uid,valueid)
	lua_register(luaState, "getPlayerStorageValue", BaseScript::luaActionGetPlayerStorageValue);
	//setPlayerStorageValue(uid,valueid, newvalue)
	lua_register(luaState, "setPlayerStorageValue", BaseScript::luaActionSetPlayerStorageValue);
	//getPlayerItemCount(uid,itemid)
	//getPlayerItem(uid,itemid)

	//setPlayerStorageValue(uid,valueid, newvalue)
	
	//getTilePzInfo(pos) 1 is pz. 0 no pz.
	lua_register(luaState, "getTilePzInfo", BaseScript::luaActionGetTilePzInfo);
	
	//getItemRWInfo(uid)
	lua_register(luaState, "getItemRWInfo", BaseScript::luaActionGetItemRWInfo);
	//getThingfromPos(pos)
	lua_register(luaState, "getThingfromPos", BaseScript::luaActionGetThingfromPos);
	
	//doRemoveItem(uid,n)
	lua_register(luaState, "doRemoveItem", BaseScript::luaActionDoRemoveItem);
	//doPlayerFeed(uid,food)
	lua_register(luaState, "doPlayerFeed", BaseScript::luaActionDoFeedPlayer);	
	//doPlayerSendCancel(uid,text)
	lua_register(luaState, "doPlayerSendCancel", BaseScript::luaActionDoSendCancel);
	//doTeleportThing(uid,newpos)
	lua_register(luaState, "doTeleportThing", BaseScript::luaActionDoTeleportThing);
	//doTransformItem(uid,toitemid)	
	lua_register(luaState, "doTransformItem", BaseScript::luaActionDoTransformItem);
	//doPlayerSay(uid,text,type)
	lua_register(luaState, "doPlayerSay", BaseScript::luaActionDoPlayerSay);
	//doPlayerSendMagicEffect(uid,position,type)
	lua_register(luaState, "doSendMagicEffect", BaseScript::luaActionDoSendMagicEffect);
	//doChangeTypeItem(uid,new_type)	
	lua_register(luaState, "doChangeTypeItem", BaseScript::luaActionDoChangeTypeItem);
	//doSetItemActionId(uid,actionid)
	lua_register(luaState, "doSetItemActionId", BaseScript::luaActionDoSetItemActionId);
	//doSetItemText(uid,text)
	lua_register(luaState, "doSetItemText", BaseScript::luaActionDoSetItemText);
	//doSetItemSpecialDescription(uid,desc)
	lua_register(luaState, "doSetItemSpecialDescription", BaseScript::luaActionDoSetItemSpecialDescription);


	//doSendAnimatedText(position,text,color)
	lua_register(luaState, "doSendAnimatedText", BaseScript::luaActionDoSendAnimatedText);
	//doPlayerAddSkillTry(uid,skillid,n)
	lua_register(luaState, "doPlayerAddSkillTry", BaseScript::luaActionDoPlayerAddSkillTry);
	//doPlayerAddHealth(uid,health)
	lua_register(luaState, "doPlayerAddHealth", BaseScript::luaActionDoPlayerAddHealth);
	//doPlayerAddMana(uid,mana)
	lua_register(luaState, "doPlayerAddMana", BaseScript::luaActionDoPlayerAddMana);
	//doPlayerAddItem(uid,itemid,count or type) . returns uid of the created item
	lua_register(luaState, "doPlayerAddItem", BaseScript::luaActionDoPlayerAddItem);
	//doPlayerSendTextMessage(uid,MessageClasses,message)
	lua_register(luaState, "doPlayerSendTextMessage", BaseScript::luaActionDoPlayerSendTextMessage);		
	//doShowTextWindow(uid,maxlen,canWrite)	
	lua_register(luaState, "doShowTextWindow", BaseScript::luaActionDoShowTextWindow);	
	//doDecayItem(uid)
	lua_register(luaState, "doDecayItem", BaseScript::luaActionDoDecayItem);
	//doCreateItem(itemid,type or count,position) .only working on ground. returns uid of the created item
	lua_register(luaState, "doCreateItem", BaseScript::luaActionDoCreateItem);
	//doSummonCreature(name, position)
	lua_register(luaState, "doSummonCreature", BaseScript::luaActionDoSummonCreature);
	
	//doPlayerChangeSpeed(uid, speedchange, time)
	lua_register(luaState, "doPlayerChangeSpeed", BaseScript::luaActionDoPlayerChangeSpeed);

	//doMoveItem(uid,toPos)
	//doMovePlayer(cid,direction)
	
	//doPlayerAddCondition(....)
	//doPlayerRemoveItem(itemid,count)
	
	return true;
}

BaseScript* BaseScript::getScript(lua_State *L){
	lua_getglobal(L, "addressOfBaseScript");
	int val = (int)internalGetNumber(L);

	BaseScript* myaction = (BaseScript*) val;
	if(!myaction){
		return 0;
	}
	return myaction;
}


Position BaseScript::internalGetRealPosition(Player *player, const Position &pos)
{
	if(pos.x == 0xFFFF){
		Position dummyPos(0,0,0);
		if(!player)
			return dummyPos;
		if(pos.y & 0x40) { //from container
			unsigned char containerid = pos.y & 0x0F;
			const Container* container = player->getContainer(containerid);
			if(!container){
				return dummyPos;
			}
			while(container->getParent() != NULL) {
				container = container->getParent();
			}
			if(container->pos.x == 0xFFFF)
				return player->pos;
			else
				return container->pos;
		}
		else //from inventory
		{
			return player->pos;
		}
	}
	else{
		return pos;
	}
}

void BaseScript::internalAddThing(lua_State *L, const Thing* thing, const unsigned int thingid)
{	
	lua_newtable(L);
	if(dynamic_cast<const Item*>(thing)){	
		const Item *item = dynamic_cast<const Item*>(thing);
		setField(L,"uid", thingid);
		setField(L,"itemid", item->getID());
		setField(L,"type", item->getItemCountOrSubtype());
		setField(L,"actionid", item->getActionId());
	}
	else if(dynamic_cast<const Creature*>(thing)){
		setField(L,"uid", thingid);
		setField(L,"itemid", 1);
		char type;
		if(dynamic_cast<const Player*>(thing)){
			type = 1;
		}
		else if(dynamic_cast<const Monster*>(thing)){
			type = 2;
		}
		else{//npc
			type = 3;
		}	
		setField(L,"type", type);
		setField(L,"actionid", 0);
	}	
	else{
		setField(L,"uid", 0);
		setField(L,"itemid", 0);
		setField(L,"type", 0);
		setField(L,"actionid", 0);
	}
}

void BaseScript::internalAddPositionEx(lua_State *L, const PositionEx& pos)
{
	lua_newtable(L);
	setField(L,"z", pos.z);
	setField(L,"y", pos.y);
	setField(L,"x", pos.x);
	setField(L,"stackpos",pos.stackpos);
}

void BaseScript::internalGetPositionEx(lua_State *L, PositionEx& pos)
{	
	pos.z = (int)getField(L,"z");
	pos.y = (int)getField(L,"y");
	pos.x = (int)getField(L,"x");
	pos.stackpos = (int)getField(L,"stackpos");
	lua_pop(L, 1); //table
}

unsigned long BaseScript::internalGetNumber(lua_State *L)
{
	lua_pop(L,1);
	return (unsigned long)lua_tonumber(L, 0);
}
const char* BaseScript::internalGetString(lua_State *L)
{	
	lua_pop(L,1);		
	return lua_tostring(L, 0);
}

int BaseScript::internalGetPlayerInfo(lua_State *L, ePlayerInfo info)
{
	unsigned int cid = (unsigned int)internalGetNumber(L);
	BaseScript *action = getScript(L);
	int value;
	
	const KnownThing* tmp = action->GetPlayerByUID(cid);
	if(tmp){
		PositionEx pos;
		Tile *tile;
		Player *player = (Player*)(tmp->thing);
		switch(info){
		case PlayerInfoAccess:
			value = player->access;
			break;		
		case PlayerInfoLevel:
			value = player->level;
			break;		
		case PlayerInfoMagLevel:
			value = player->maglevel;
			break;
		case PlayerInfoMana:
			value = player->mana;
			break;
		case PlayerInfoHealth:
			value = player->level;
			break;
		case PlayerInfoName:
			lua_pushstring(L, player->name.c_str());
			return 1;
			break;
		case PlayerInfoPosition:			
			pos = player->pos;
			tile = action->game->map->getTile(player->pos.x, player->pos.y, player->pos.z);
			if(tile)
				pos.stackpos = tile->getCreatureStackPos(player);
			internalAddPositionEx(L,pos);
			return 1;
			break;
		case PlayerInfoFood:
			value = player->food/1000;
			break;

		case PlayerInfoSpeed:
			value = player->getNormalSpeed();
			break;

		default:
			std::cout << "GetPlayerInfo: Unkown player info " << info << std::endl;
			value = 0;
			break;		
		}
		lua_pushnumber(L,value);
		return 1;
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "GetPlayerInfo(" << info << "): player not found" << std::endl;
		return 1;
	}		
	
	lua_pushnumber(L, 0);
	return 1;
}
//getPlayer[Info](uid)
int BaseScript::luaActionGetPlayerFood(lua_State *L){	
	return internalGetPlayerInfo(L,PlayerInfoFood);}
	
int BaseScript::luaActionGetPlayerAccess(lua_State *L){
	return internalGetPlayerInfo(L,PlayerInfoAccess);}
	
int BaseScript::luaActionGetPlayerLevel(lua_State *L){
	return internalGetPlayerInfo(L,PlayerInfoLevel);}
	
int BaseScript::luaActionGetPlayerMagLevel(lua_State *L){
	return internalGetPlayerInfo(L,PlayerInfoMagLevel);}
	
int BaseScript::luaActionGetPlayerMana(lua_State *L){
	return internalGetPlayerInfo(L,PlayerInfoMana);}

int BaseScript::luaActionGetPlayerHealth(lua_State *L){
	return internalGetPlayerInfo(L,PlayerInfoHealth);}
	
int BaseScript::luaActionGetPlayerName(lua_State *L){
	return internalGetPlayerInfo(L,PlayerInfoName);}
	
int BaseScript::luaActionGetPlayerPosition(lua_State *L){	
	return internalGetPlayerInfo(L,PlayerInfoPosition);
}

int BaseScript::luaActionGetPlayerSpeed(lua_State *L){
	return internalGetPlayerInfo(L, PlayerInfoSpeed);}

//

int BaseScript::luaActionDoRemoveItem(lua_State *L)
{	
	//doRemoveItem(uid,n)
	char n = (unsigned char)internalGetNumber(L);	
	unsigned short itemid = (unsigned short)internalGetNumber(L);
						
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetItemByUID(itemid);
	Item *tmpitem = NULL;
	PositionEx tmppos;
	if(tmp){
		tmpitem = (Item*)tmp->thing;
		tmppos = tmp->pos;
		if(tmpitem->isSplash()){
			lua_pushnumber(L, -1);
			std::cout << "luaDoRemoveItem: can not remove a splash" << std::endl;
			return 1;
		}
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luaDoRemoveItem: item not found" << std::endl;
		return 1;
	}
	
	if(tmpitem->isStackable() && (tmpitem->getItemCountOrSubtype() - n) > 0){
		tmpitem->setItemCountOrSubtype(tmpitem->getItemCountOrSubtype() - n);
		action->game->sendUpdateThing(action->_player,(Position&)tmppos,tmpitem,tmppos.stackpos);
	}
	else{
		action->game->removeThing(action->_player,(Position&)tmppos,tmpitem);
		action->game->FreeThing(tmpitem);
	}	
	
	lua_pushnumber(L, 0);
	return 1;
}


int BaseScript::luaActionDoFeedPlayer(lua_State *L)
{	
	//doFeedPlayer(uid,food)
	int food = (int)internalGetNumber(L);
	unsigned int cid = (unsigned int)internalGetNumber(L);
	
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetPlayerByUID(cid);
	if(tmp){
		Player *player = (Player*)(tmp->thing);
		player->food += food*1000;
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luaDoFeedPlayer: player not found" << std::endl;
		return 1;
	}		
	
	lua_pushnumber(L, 0);
	return 1;
}

int BaseScript::luaActionDoSendCancel(lua_State *L)
{	
	//doSendCancel(uid,text)
	const char * text = internalGetString(L);
	unsigned int cid = (unsigned int)internalGetNumber(L);	
	
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetPlayerByUID(cid);
	if(tmp){
		Player *player = (Player*)(tmp->thing);
		player->sendCancel(text);
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luaSendCancel: player not found" << std::endl;
		return 1;
	}		
	lua_pushnumber(L, 0);
	return 1;
}


int BaseScript::luaActionDoTeleportThing(lua_State *L)
{
	//doTeleportThing(uid,newpos)
	PositionEx pos;
	internalGetPositionEx(L,pos);
	unsigned int id = (unsigned int)internalGetNumber(L);	
	
	BaseScript *action = getScript(L);
	Thing *tmpthing;
	
	const KnownThing* tmp = action->GetThingByUID(id);
	if(tmp){
		tmpthing = tmp->thing;		
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luaTeleport: thing not found" << std::endl;
		return 1;
	}
	//std::cout << "new pos: " << (Position&)pos << std::endl;
	if(tmp->type == thingTypeItem){
		//avoid teleport notMoveable items
		if(((Item*)tmp->thing)->isNotMoveable()){
			lua_pushnumber(L, -1);
			std::cout << "luaTeleport: item is not moveable" << std::endl;
			return 1;
		}
	}
	
	action->game->teleport(tmpthing,(Position&)pos);
	Tile *tile = action->game->getTile(pos.x, pos.y, pos.z);
	if(tile){
		pos.stackpos = tile->getThingStackPos(tmpthing);
	}
	else{
		pos.stackpos = 1;
	}
	action->UpdateThingPos(id,pos);
	
	lua_pushnumber(L, 0);
	return 1;
}

	
int BaseScript::luaActionDoTransformItem(lua_State *L)
{
	//doTransformItem(uid,toitemid)	
	unsigned int toid = (unsigned int)internalGetNumber(L);	
	unsigned int itemid = (unsigned int)internalGetNumber(L);	
	
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetItemByUID(itemid);	
	Item *tmpitem = NULL;
	PositionEx tmppos;
	if(tmp){
		tmpitem = (Item*)tmp->thing;
		tmppos = tmp->pos;		
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luaDoTransform: Item not found" << std::endl;
		return 1;
	}
	
	tmpitem->setID(toid);
	
	action->game->sendUpdateThing(action->_player,(Position&)tmppos,tmpitem,tmppos.stackpos);
	
	lua_pushnumber(L, 0);
	return 1;
}

int BaseScript::luaActionDoPlayerSay(lua_State *L)
{
	//doPlayerSay(uid,text,type)
	int type = (int)internalGetNumber(L);	
	const char * text = internalGetString(L);
	unsigned int cid = (unsigned int)internalGetNumber(L);	
					
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetPlayerByUID(cid);
	if(tmp){
		Player *player = (Player*)(tmp->thing);
		action->game->creatureSay(player,(SpeakClasses)type,std::string(text));
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luaDoPlayerSay: player not found" << std::endl;
		return 1;
	}		
		
	lua_pushnumber(L, 0);
	return 1;
}

int BaseScript::luaActionDoSendMagicEffect(lua_State *L)
{
	//doSendMagicEffect(position,type)
	int type = (int)internalGetNumber(L);	
	PositionEx pos;
	internalGetPositionEx(L,pos);
	
	BaseScript *action = getScript(L);
	
	Position realpos = internalGetRealPosition(action->_player,(Position&)pos);
	std::vector<Creature*> list;
	action->game->getSpectators(Range(realpos, true), list);
	for(unsigned int i = 0; i < list.size(); ++i){
		Player *p = dynamic_cast<Player*>(list[i]);
		if(p)
			p->sendMagicEffect(realpos,type);
	}	
	
	lua_pushnumber(L, 0);
	return 1;
}

int BaseScript::luaActionDoChangeTypeItem(lua_State *L)
{
	//doChangeTypeItem(uid,new_type)
	unsigned int new_type = (unsigned int)internalGetNumber(L);	
	unsigned int itemid = (unsigned int)internalGetNumber(L);
	
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetItemByUID(itemid);
	Item *tmpitem = NULL;
	PositionEx tmppos;
	if(tmp){
		tmpitem = (Item*)tmp->thing;
		tmppos = tmp->pos;		
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luaDoChangeTypeItem: Item not found" << std::endl;
		return 1;
	}
	
	tmpitem->setItemCountOrSubtype(new_type);
	
	action->game->sendUpdateThing(action->_player,(Position&)tmppos,tmpitem,tmppos.stackpos);
	
	lua_pushnumber(L, 0);
	return 1;		
}


int BaseScript::luaActionDoPlayerAddSkillTry(lua_State *L)
{
	//doPlayerAddSkillTry(uid,skillid,n)
	int n = (int)internalGetNumber(L);
	int skillid = (int)internalGetNumber(L);
	unsigned int cid = (unsigned int)internalGetNumber(L);	
					
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetPlayerByUID(cid);
	if(tmp){
		Player *player = (Player*)(tmp->thing);
		player->addSkillTryInternal(n,skillid);
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luaDoPlayerAddSkillTry: player not found" << std::endl;
		return 1;
	}		
		
	lua_pushnumber(L, 0);
	return 1;
}


int BaseScript::luaActionDoPlayerAddHealth(lua_State *L)
{
	//doPlayerAddHealth(uid,health)
	int addhealth = (int)internalGetNumber(L);
	unsigned int cid = (unsigned int)internalGetNumber(L);	
					
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetPlayerByUID(cid);
	if(tmp){
		Player *player = (Player*)(tmp->thing);
		player->health = std::min(player->healthmax,player->health+addhealth);
		player->sendStats();
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luaDoPlayerAddHealth: player not found" << std::endl;
		return 1;
	}		
		
	lua_pushnumber(L, 0);
	return 1;
}

int BaseScript::luaActionDoPlayerAddMana(lua_State *L)
{
	//doPlayerAddMana(uid,mana)
	int addmana = (int)internalGetNumber(L);
	unsigned int cid = (unsigned int)internalGetNumber(L);	
					
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetPlayerByUID(cid);
	if(tmp){
		Player *player = (Player*)(tmp->thing);
		player->mana = std::min(player->manamax,player->mana+addmana);
		player->sendStats();
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luaDoPlayerAddMana: player not found" << std::endl;
		return 1;
	}		
		
	lua_pushnumber(L, 0);
	return 1;
}

int BaseScript::luaActionDoPlayerAddItem(lua_State *L)
{
	//doPlayerAddItem(uid,itemid,count or type)
	int type = (int)internalGetNumber(L);
	int itemid = (int)internalGetNumber(L);
	unsigned int cid = (unsigned int)internalGetNumber(L);	
	
	BaseScript *action = getScript(L);
	unsigned int uid;
	const KnownThing* tmp = action->GetPlayerByUID(cid);
	if(tmp){
		PositionEx pos;
		Player *player = (Player*)(tmp->thing);
		Item *newitem = Item::CreateItem(itemid,type);
		if(!player->addItem(newitem)){
			//add item on the ground
			action->game->addThing(NULL,action->_player->pos,newitem);
			Tile *tile = action->game->getTile(newitem->pos.x, newitem->pos.y, newitem->pos.z);
			if(tile){
				pos.stackpos = tile->getThingStackPos(newitem);
			}
			else{
				pos.stackpos = 1;
			}
		}
		pos.x = newitem->pos.x;
		pos.y = newitem->pos.y;
		pos.z = newitem->pos.z;
		uid = action->AddThingToMap((Thing*)newitem,pos);
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luaDoPlayerAddItem: player not found" << std::endl;
		return 1;
	}		
		
	lua_pushnumber(L, uid);
	return 1;
}


int BaseScript::luaActionDoPlayerSendTextMessage(lua_State *L)
{	
	//doPlayerSendTextMessage(uid,MessageClasses,message)
	const char * text = internalGetString(L);
	unsigned char messageClass = (unsigned char)internalGetNumber(L);	
	unsigned int cid = (unsigned int)internalGetNumber(L);	
	
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetPlayerByUID(cid);
	if(tmp){
		Player *player = (Player*)(tmp->thing);
		player->sendTextMessage((MessageClasses)messageClass,text);;
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luaSendTextMessage: player not found" << std::endl;
		return 1;
	}		
	lua_pushnumber(L, 0);
	return 1;
}

int BaseScript::luaActionDoSendAnimatedText(lua_State *L)
{	
	//doSendAnimatedText(position,text,color)
	int color = (int)internalGetNumber(L);
	const char * text = internalGetString(L);
	PositionEx pos;
	internalGetPositionEx(L,pos);
	
	BaseScript *action = getScript(L);
	
	Position realpos = internalGetRealPosition(action->_player,(Position&)pos);
	std::vector<Creature*> list;
	action->game->getSpectators(Range(realpos, true), list);
	for(unsigned int i = 0; i < list.size(); ++i){
		Player *p = dynamic_cast<Player*>(list[i]);
		if(p)
			p->sendAnimatedText(realpos, color, text);
	}
	
	lua_pushnumber(L, 0);
	return 1;
}

int BaseScript::luaActionGetPlayerSkill(lua_State *L)
{
	//getPlayerSkill(uid,skillid)
	unsigned char skillid = (unsigned int)internalGetNumber(L);
	unsigned int cid = (unsigned int)internalGetNumber(L);
	
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetPlayerByUID(cid);
	if(tmp){
		if(skillid > 6){
			lua_pushnumber(L, -1);
			std::cout << "GetPlayerSkill: invalid skillid" << std::endl;
			return 1;
		}
		Player *player = (Player*)(tmp->thing);
		int value = player->skills[skillid][SKILL_LEVEL];
		lua_pushnumber(L,value);
		return 1;
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "GetPlayerSkill: player not found" << std::endl;
		return 1;
	}
}

int BaseScript::luaActionDoShowTextWindow(lua_State *L){
	//doShowTextWindow(uid,maxlen,canWrite)
	bool canWrite = (bool)internalGetNumber(L);
	unsigned short maxlen = (unsigned short)internalGetNumber(L);
	unsigned int uid = (unsigned int)internalGetNumber(L);
	
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetItemByUID(uid);
	Item *tmpitem = NULL;
	if(tmp){
		tmpitem = (Item*)tmp->thing;
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luadoShowTextWindow: Item not found" << std::endl;
		return 1;
	}
	
	action->_player->sendTextWindow(tmpitem,maxlen,canWrite);
	
	lua_pushnumber(L, 0);
	return 1;
}

int BaseScript::luaActionGetItemRWInfo(lua_State *L)
{
	//getItemRWInfo(uid)
	unsigned int uid = (unsigned int)internalGetNumber(L);
	
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetItemByUID(uid);
	Item *tmpitem = NULL;
	if(tmp){
		tmpitem = (Item*)tmp->thing;
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luagetItemRWInfo: Item not found" << std::endl;
		return 1;
	}
	
	lua_pushnumber(L, (int)tmpitem->getRWInfo());
	
	return 1;
}

int BaseScript::luaActionDoDecayItem(lua_State *L){
	//doDecayItem(uid)
	//Note: to stop decay set decayTo = 0 in items.xml
	unsigned int uid = (unsigned int)internalGetNumber(L);	
	
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetItemByUID(uid);
	Item *tmpitem = NULL;
	if(tmp){
		tmpitem = (Item*)tmp->thing;
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luadoDecayItem: Item not found" << std::endl;
		return 1;
	}
	
	action->game->startDecay(tmpitem);
	
	lua_pushnumber(L, 0);
	return 1;
}

int BaseScript::luaActionGetThingfromPos(lua_State *L)
{
	//getThingfromPos(pos)
	//Note: 
	//	stackpos = 255. Get the top thing(item moveable or creature).
	//	stackpos = 254. Get MagicFieldtItem
	//	stackpos = 253. Get Creature
	
	PositionEx pos;
	internalGetPositionEx(L,pos);
	
	BaseScript *action = getScript(L);
	
	Tile *tile = action->game->getTile(pos.x, pos.y, pos.z);
	Thing *thing = NULL;
	
	if(tile){
		if(pos.stackpos == 255){
			thing = tile->getTopMoveableThing();
		}
		else if(pos.stackpos == 254){
			thing = NULL; //tile->getFieldItem();
		}
		else if(pos.stackpos == 253){
			thing = tile->getTopCreature();
		}
		else{
			thing = tile->getThingByStackPos(pos.stackpos);
		}
		
		if(thing){
			if(pos.stackpos > 250){
				pos.stackpos = tile->getThingStackPos(thing);
			}
			unsigned int thingid = action->AddThingToMap(thing,pos);
			internalAddThing(L,thing,thingid);
		}
		else{
			internalAddThing(L,NULL,0);	
		}
		return 1;
		
	}//if(tile)
	else{
		std::cout << "luagetItemfromPos: Tile not found" << std::endl;
		internalAddThing(L,NULL,0);
		return 1;
	}
}

int BaseScript::luaActionDoCreateItem(lua_State *L){
	//doCreateItem(itemid,type or count,position) .only working on ground. returns uid of the created item
	PositionEx pos;
	internalGetPositionEx(L,pos);
	int type = (int)internalGetNumber(L);
	int itemid = (int)internalGetNumber(L);
	
	BaseScript *action = getScript(L);
	
	Item *newitem = Item::CreateItem(itemid,type);
	action->game->addThing(NULL,(Position&)pos,newitem);
	Tile *tile = action->game->getTile(pos.x, pos.y, pos.z);
	if(tile){
		pos.stackpos = tile->getThingStackPos(newitem);
	}
	else{
		pos.stackpos = 1;
	}
	
	unsigned int uid = action->AddThingToMap((Thing*)newitem,pos);
	
	lua_pushnumber(L, uid);
	return 1;	
}

int BaseScript::luaActionGetPlayerStorageValue(lua_State *L)
{
	//getPlayerStorageValue(cid,valueid)
	unsigned long key = (unsigned int)internalGetNumber(L);
	unsigned int cid = (unsigned int)internalGetNumber(L);
	
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetPlayerByUID(cid);
	if(tmp){
		Player *player = (Player*)(tmp->thing);
		long value;
		if(player->getStorageValue(key,value)){
			lua_pushnumber(L,value);
		}
		else{
			lua_pushnumber(L,-1);
		}
		return 1;
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "GetPlayerStorageValue: player not found" << std::endl;
		return 1;
	}
}

int BaseScript::luaActionSetPlayerStorageValue(lua_State *L)
{
	//setPlayerStorageValue(cid,valueid, newvalue)
	long value = (unsigned int)internalGetNumber(L);
	unsigned long key = (unsigned int)internalGetNumber(L);
	unsigned int cid = (unsigned int)internalGetNumber(L);
	
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetPlayerByUID(cid);
	if(tmp){
		Player *player = (Player*)(tmp->thing);
		player->addStorageValue(key,value);
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "SetPlayerStorageValue: player not found" << std::endl;
		return 1;
	}
	lua_pushnumber(L,0);
	return 1;
}

int BaseScript::luaActionDoSetItemActionId(lua_State *L)
{
	//doSetItemActionId(uid,actionid)
	unsigned int actionid = (unsigned int)internalGetNumber(L);	
	unsigned int itemid = (unsigned int)internalGetNumber(L);
	
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetItemByUID(itemid);	
	Item *tmpitem = NULL;
	PositionEx tmppos;
	if(tmp){
		tmpitem = (Item*)tmp->thing;
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luaDoSetActionId: Item not found" << std::endl;
		return 1;
	}
	
	tmpitem->setActionId(actionid);
	
	lua_pushnumber(L, 0);
	return 1;
}

int BaseScript::luaActionDoSetItemText(lua_State *L)
{
	//doSetItemText(uid,text)
	const char *text = internalGetString(L);
	unsigned int itemid = (unsigned int)internalGetNumber(L);	
	
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetItemByUID(itemid);
	Item *tmpitem = NULL;
	PositionEx tmppos;
	if(tmp){
		tmpitem = (Item*)tmp->thing;
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luaDoSetText: Item not found" << std::endl;
		return 1;
	}
	
	tmpitem->setText(text);
	
	lua_pushnumber(L, 0);
	return 1;
}

int BaseScript::luaActionDoSetItemSpecialDescription(lua_State *L)
{
	//doSetItemSpecialDescription(uid,desc)
	const char *desc = internalGetString(L);
	unsigned int itemid = (unsigned int)internalGetNumber(L);
	
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetItemByUID(itemid);
	Item *tmpitem = NULL;
	PositionEx tmppos;
	if(tmp){
		tmpitem = (Item*)tmp->thing;
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luaDoSetSpecialDescription: Item not found" << std::endl;
		return 1;
	}
	
	tmpitem->setSpecialDescription(desc);
	
	lua_pushnumber(L, 0);
	return 1;
}

int BaseScript::luaActionGetTilePzInfo(lua_State *L)
{
	//getTilePzInfo(pos)
	PositionEx pos;
	internalGetPositionEx(L,pos);
	
	BaseScript *action = getScript(L);
	
	Tile *tile = action->game->getTile(pos.x, pos.y, pos.z);
	
	if(tile){
		if(tile->isPz()){
			lua_pushnumber(L, 1);
		}
		else{
			lua_pushnumber(L, 0);
		}
	}//if(tile)
	else{
		std::cout << "luagetTilePzInfo: Tile not found" << std::endl;
		lua_pushnumber(L, -1);
	}
	return 1;
}

int BaseScript::luaActionDoSummonCreature(lua_State *L){
	//doSummonCreature(name, position)
	PositionEx pos;
	internalGetPositionEx(L,pos);
	const char *name = internalGetString(L);
	
	BaseScript *action = getScript(L);
	
	Monster *monster = new Monster(name, action->game);
	if(!monster->isLoaded()){
		delete monster;
		lua_pushnumber(L, 0);
		std::cout << "luadoSummonCreature: Monster not found" << std::endl;
		return 1;
	}
	
	if(!action->game->placeCreature((Position&)pos, monster)) {
		delete monster;
		lua_pushnumber(L, 0);
		std::cout << "luadoSummonCreature: Can not place the monster" << std::endl;
		return 1;
	}
	
	unsigned int cid = action->AddThingToMap((Thing*)monster,pos);
	
	lua_pushnumber(L, cid);
	return 1;	
}


int BaseScript::luaActionDoPlayerChangeSpeed(lua_State *L){
	//doPlayerChangeSpeed(uid, speedchange, time)
	int time = internalGetNumber(L) * 1000;
	int speedchange = internalGetNumber(L);
	unsigned int cid = (unsigned int)internalGetNumber(L);
	
	BaseScript *action = getScript(L);
	
	const KnownThing* tmp = action->GetCreatureByUID(cid);
	if(tmp){
		//CONDITION_HASTE and CONDITION_PARALYZE
		// are equivalent in Condition::createCondition
		// final condition type is decided depending on the sign of speedchange
		Condition *cond = Condition::createCondition(CONDITION_HASTE, time, speedchange);
		((Creature*)(tmp->thing))->addCondition(cond);
		
		
		/*
		Player *player = (Player*)(tmp->thing);
		spell->game->addEvent(makeTask(time, boost::bind(&Game::changeSpeed, spell->game,creature->getID(), creature->getNormalSpeed()) ) );
		Player* p = dynamic_cast<Player*>(creature);
		if(p) {
			spell->game->changeSpeed(creature->getID(), creature->getNormalSpeed()+speed); 
			p->sendIcons();
		}
		
		creature->hasteTicks = time;  
		*/
	}
	else{
		lua_pushnumber(L, -1);
		std::cout << "luaDoPlayerChangeSpeed: player not found" << std::endl;
		return 1;
	}		
	
	lua_pushnumber(L, 0);
	return 1;
}

ActionScript::ActionScript(Game *igame,const std::string &datadir,const std::string &scriptname) :
	BaseScript(igame)
{
  lua_dofile(luaState, std::string(datadir + "actions/lib/actions.lua").c_str());
	loadScript(scriptname);
}

