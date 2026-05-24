#pragma once

#include "unit.h"

#include <memory>
#include <string>
#include <vector>

using namespace std;

struct UnitTemplate {
    string name;
    string profession;
    int maxHp;
    int atk;
    int defense;
    int magicResist;
    int price;
};

const vector<UnitTemplate>& GetUnitTemplates();
shared_ptr<Unit> CreateUnitByName(const string& name, int star = 1);
const UnitTemplate* FindUnitTemplate(const string& name);
