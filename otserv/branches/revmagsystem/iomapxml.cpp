//////////////////////////////////////////////////////////////////////
// OpenTibia - an opensource roleplaying game
//////////////////////////////////////////////////////////////////////
// XML implementation of the Map Loader/Saver
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

#include "iomapxml.h"

#include <iostream>

bool IOMapXML::loadMap(Map* map, std::string identifier){
	xmlDocPtr doc;
	xmlNodePtr root, tile, p, tmpNode;
	int width, height;

	xmlLineNumbersDefault(1);
	std::cout << "loaded map " << identifier << std::endl;
	doc=xmlParseFile(identifier.c_str());
	if (!doc) {
		std::cout << "FATAL: couldnt load map. exiting" << std::endl;
		exit(1);
	}
	root=xmlDocGetRootElement(doc);
	if(xmlStrcmp(root->name,(const xmlChar*) "map")){
		xmlFreeDoc(doc);
		std::cout << "FATAL: couldnt load map. exiting" << std::endl;
		exit(1);
	}

	width=atoi((const char*)xmlGetProp(root, (const xmlChar *) "width"));
	height=atoi((const char*)xmlGetProp(root, (const xmlChar *) "height"));
	std::cout << width << "  " << height << std::endl;

	std::string spawnfile = "";
	if(xmlGetProp(root, (const xmlChar *) "spawnfile")) {
		map->spawnfile = identifier.substr(0, identifier.rfind('/') + 1);
		map->spawnfile += (const char*)xmlGetProp(root, (const xmlChar *) "spawnfile");
	}

	tile=root->children;

	int px,py,pz;
  char* tmp;
  Tile *t;

  while (tile) { 
      tmp = (char*)xmlGetProp(tile, (const xmlChar *) "x");
			if(!tmp) {
				tile=tile->next;
				continue;
			}

      px = atoi(tmp);
      tmp = (char*)xmlGetProp(tile, (const xmlChar *) "y");
      py = atoi(tmp);

      tmp = (char*)xmlGetProp(tile, (const xmlChar *) "z");
      pz = atoi(tmp);
      
			tmp = (char*)xmlGetProp(tile, (const xmlChar *) "ground");
/*
#ifdef __DEBUG__
			if(tmp == NULL) {
				std::cout << "No ground tile! x: " << px << ", y: " << py << " z: " << pz << std::endl;
			}
#endif
*/

			unsigned short ground = 0;

			if(tmp) {
				ground = atoi(tmp);
			}

      map->setTile(px,py,pz,ground);
      t = map->getTile(px,py,pz);
      
      tmp = (char*)xmlGetProp(tile, (const xmlChar *) "pz");
      if (tmp && (strcmp(tmp, "1") == 0)) t->setPz();
       
      p = tile->children;
      while(p) {
               
        if(xmlStrcmp(p->name,(const xmlChar*) "item")==0){          
			unsigned int id = atoi((const char*)xmlGetProp(p, (const xmlChar *) "id"));
			Item* myitem = Item::CreateItem(id);
			myitem->unserialize(p);
			Container *container = dynamic_cast<Container*>(myitem);
			if(container){
				//is depot?					
				tmp = (char*)xmlGetProp(p, (const xmlChar *) "depot");
				if(tmp){					
					int depotnumber = atoi(tmp);					
					container->depot = depotnumber;
				}
				if(p->children && strcmp((const char*)p->children->name, "inside") == 0){
					tmpNode = p->children->children;
					while(tmpNode){
						unsigned int id = atoi((const char*)xmlGetProp(tmpNode, (const xmlChar *) "id"));
						Item* myitem = Item::CreateItem(id);
						myitem->unserialize(tmpNode);
						container->addItem(myitem);
			
						Container* in_container = dynamic_cast<Container*>(myitem);
						if(in_container){
							LoadContainer(tmpNode,in_container);
						}
						tmpNode = tmpNode->next;
					}
				}
				//loadContainer
			}

			myitem->pos.x = px;
			myitem->pos.y = py;
			myitem->pos.z = pz;		
			
      		if (myitem->isAlwaysOnTop())
				t->topItems.push_back(myitem);
			else
				t->downItems.push_back(myitem);
			}
				
			p=p->next;
		}
      tile=tile->next;
    }
  xmlFreeDoc(doc);
	
	return true;
}

bool IOMapXML::LoadContainer(xmlNodePtr nodeitem,Container* ccontainer)
{
	xmlNodePtr tmp,p;
	if(nodeitem==NULL){
		return false;
	}
	tmp=nodeitem->children;
	if(tmp==NULL){
		return false;
	}
                  
	if (strcmp((const char*)tmp->name, "inside") == 0){
		//load items
		p=tmp->children;
		while(p){			
			unsigned int id = atoi((const char*)xmlGetProp(p, (const xmlChar *) "id"));
			Item* myitem = Item::CreateItem(id);
			myitem->unserialize(p);			
			ccontainer->addItem(myitem);
			
			Container* in_container = dynamic_cast<Container*>(myitem);
			if(in_container){
				LoadContainer(p,in_container);
			}
			p=p->next;
		}

		return true;
	}

	return false;
}
