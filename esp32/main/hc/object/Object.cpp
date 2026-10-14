#include "Object.h"

using namespace std;

Object::Object(string id, uint16_t addr, string name)
{
	this->id = id;
	this->addr = addr;
	this->name = name;
}

Object::~Object()
{
}

string Object::GetId()
{
	return id;
}

void Object::SetId(string id)
{
	this->id = id;
}

string Object::GetName()
{
	return name;
}

void Object::SetName(string name)
{
	this->name = name;
}

uint16_t Object::GetAddr()
{
	return addr;
}

void Object::SetAddr(uint16_t addr)
{
	this->addr = addr;
}
