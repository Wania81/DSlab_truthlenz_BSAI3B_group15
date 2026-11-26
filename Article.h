#pragma once
#include <string>
using namespace std;

struct Article {
    int id;
    string title;
    string source;   // domain, e.g. dawn.com
    string author;   // may be empty
    string date;     // may be empty
    string text;
    int score = 100;
    string label;    // Credible / Questionable / Misleading
};
