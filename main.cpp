// TruthLens - AI-Based News Credibility Analyzer (DS Lab project)
//
// Data structures used (all hand-written, see the .h files):
//   LinkedList : hash chaining, flags, history, graph adjacency, BFS queue
//   HashTable  : id -> Article, domain -> reputation, keyword -> article IDs, id -> flags
//   BST        : articles ordered by credibility score (range search)
//   Graph      : related stories (BFS) and story clusters (DFS)
//   MinHeap    : top-K most suspicious articles
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "Analyzer.h"
#include "Article.h"
#include "BST.h"
#include "Graph.h"
#include "HashTable.h"
#include "LinkedList.h"
#include "MinHeap.h"
using namespace std;

// ======================= input / text helpers =======================
static bool g_eof = false;

static string readLine(const string& prompt) {
    cout << prompt;
    string s;
    if (!getline(cin, s)) {
        g_eof = true;
        return "";
    }
    return s;
}

static string trim(const string& s) {
    size_t b = 0, e = s.size();
    while (b < e && isspace((unsigned char)s[b])) ++b;
    while (e > b && isspace((unsigned char)s[e - 1])) --e;
    return s.substr(b, e - b);
}

// Asks until the user types a whole number in [lo, hi]. false on end of input.
static bool readInt(const string& prompt, int lo, int hi, int& out) {
    while (true) {
        string s = trim(readLine(prompt));
        if (g_eof) return false;
        bool ok = !s.empty() && s.size() < 9;
        for (size_t i = 0; ok && i < s.size(); ++i)
            if (!isdigit((unsigned char)s[i])) ok = false;
        if (ok) {
            int v = stoi(s);
            if (v >= lo && v <= hi) {
                out = v;
                return true;
            }
        }
        cout << "  Please enter a number between " << lo << " and " << hi << ".\n";
    }
}

static bool allDigits(const string& s) {
    if (s.empty()) return false;
    for (size_t i = 0; i < s.size(); ++i)
        if (!isdigit((unsigned char)s[i])) return false;
    return true;
}

static string clip(const string& s, size_t n) {
    return s.size() <= n ? s : s.substr(0, n - 3) + "...";
}

// "https://www.Dawn.com/news/1" -> "dawn.com"
static string normalizeDomain(const string& raw) {
    string d = toLower(trim(raw));
    if (d.rfind("https://", 0) == 0) d = d.substr(8);
    else if (d.rfind("http://", 0) == 0) d = d.substr(7);
    if (d.rfind("www.", 0) == 0) d = d.substr(4);
    size_t slash = d.find('/');
    if (slash != string::npos) d = d.substr(0, slash);
    return d;
}


static void splitPipes(const string& line, LinkedList<string>& parts) {
    parts.clear();
    string cur;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == '|') {
            parts.insertBack(cur);
            cur.clear();
        } else {
            cur += line[i];
        }
    }
    parts.insertBack(cur);
}

struct HistoryEntry {
    int seq;
    string action;
    int id;
    string title;
    int score;
    string label;
};

// ======================= the application =======================
class TruthLens {
    HashTable<int, Article> articles;                // id -> Article
    HashTable<string, int> reputation;               // domain -> reputation (0-100)
    HashTable<string, LinkedList<int>> keywordIndex; // keyword -> article IDs
    HashTable<int, LinkedList<string>> flagsOf;      // id -> triggered indicators
    BST scoreTree;                                   // articles ordered by score
    Graph related;                                   // related stories
    LinkedList<HistoryEntry> history;                // newest first
    int nextId = 1;
    int historySeq = 0;
    string baseDir;                                  // folder that contains data/ (project root)

    // ---------- loading ----------
    bool loadSources(const string& path) {
        ifstream in(path);
        if (!in) return false;
        string line;
        while (getline(in, line)) {
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            LinkedList<string> p;
            splitPipes(line, p);
            if (p.size() < 2) continue;
            string domain = normalizeDomain(p.getHead()->data);
            string num = trim(p.getHead()->next->data);
            if (!domain.empty() && allDigits(num) && num.size() < 4) reputation.insert(domain, stoi(num));
        }
        return true;
    }

    bool loadArticles(const string& path, LinkedList<int>& loadedIds) {
        ifstream in(path);
        if (!in) return false;
        string line;
        int lineNo = 0;
        while (getline(in, line)) {
            ++lineNo;
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            LinkedList<string> p;
            splitPipes(line, p);
            if (p.size() < 6) {
                cout << "  (skipping line " << lineNo << ": expected 6 fields)\n";
                continue;
            }
            string f[6];
            LinkedList<string>::Node* cur = p.getHead();
            for (int i = 0; i < 6; ++i, cur = cur->next) f[i] = cur->data;
            for (; cur != nullptr; cur = cur->next) f[5] += "|" + cur->data;   // text may contain '|'
            if (!allDigits(trim(f[0])) || trim(f[0]).size() > 8) {
                cout << "  (skipping line " << lineNo << ": bad id)\n";
                continue;
            }
            Article a;
            a.id = stoi(trim(f[0]));
            a.title = trim(f[1]);
            a.source = normalizeDomain(f[2]);
            a.author = trim(f[3]);
            a.date = trim(f[4]);
            a.text = trim(f[5]);
            if (articles.contains(a.id)) {
                cout << "  (skipping line " << lineNo << ": duplicate id " << a.id << ")\n";
                continue;
            }
            registerArticle(a);
            loadedIds.insertBack(a.id);
        }
        return true;
    }

    // ---------- core operations ----------
    // Stores the article, indexes its keywords and connects it in the graph.
    void registerArticle(const Article& a) {
        articles.insert(a.id, a);
        if (a.id >= nextId) nextId = a.id + 1;

        LinkedList<string> kws;
        extractKeywords(a, kws);

        // count shared keywords with every article already indexed
        HashTable<int, int> shared;
        kws.forEach([&](const string& kw) {
            LinkedList<int>* ids = keywordIndex.search(kw);
            if (ids == nullptr) return;
            ids->forEach([&](int other) {
                int* c = shared.search(other);
                if (c) ++(*c);
                else shared.insert(other, 1);
            });
        });

        related.addNode(a.id);
        shared.forEach([&](const int& other, int& cnt) {
            if (cnt >= MIN_SHARED_KEYWORDS) related.addEdge(a.id, other);
        });

        // now add this article to the keyword index
        kws.forEach([&](const string& kw) {
            LinkedList<int>* ids = keywordIndex.search(kw);
            if (ids) ids->insertBack(a.id);
            else {
                LinkedList<int> fresh;
                fresh.insertBack(a.id);
                keywordIndex.insert(kw, fresh);
            }
        });
    }

    Evidence buildEvidence(const Article& a) {
        Evidence ev;
        const int* rep = reputation.search(a.source);
        if (rep) {
            ev.reputation = *rep;
            ev.sourceKnown = true;
        }
        const LinkedList<int>* nb = related.neighbors(a.id);
        if (nb) {
            nb->forEach([&](int nid) {
                const Article* other = articles.search(nid);
                if (other == nullptr || other->source == a.source) return;
                const int* r = reputation.search(other->source);
                if (r && *r >= TRUSTED_REPUTATION) ++ev.corroborations;
            });
        }
        return ev;
    }

    // Scores one article and keeps BST + flags table in sync.
    AnalysisResult analyze(int id, bool record, const string& action) {
        Article* a = articles.search(id);
        AnalysisResult res;
        if (a == nullptr) return res;

        if (flagsOf.contains(id)) scoreTree.remove(a->score, id);   // was scored before -> remove old entry
        res = scoreArticle(*a, buildEvidence(*a));
        flagsOf.insert(id, res.flags);
        scoreTree.insert(a->score, id);

        if (record) {
            HistoryEntry h;
            h.seq = ++historySeq;
            h.action = action;
            h.id = id;
            h.title = a->title;
            h.score = a->score;
            h.label = a->label;
            history.insertFront(h);
        }
        return res;
    }

    // ---------- printing ----------
    void printLine(const Article& a) const {
        cout << "  [" << setw(2) << a.id << "] " << left << setw(13) << a.label << right << setw(3) << a.score
             << "  " << clip(a.title, 50) << "  (" << a.source << ")\n";
    }

    void printLineById(int id) {
        const Article* a = articles.search(id);
        if (a) printLine(*a);
    }

    void printFlags(int id, const string& indent) {
        const LinkedList<string>* f = flagsOf.search(id);
        if (f && f->size() > 0) cout << indent << "Flags: " << joinList(*f) << "\n";
    }

    void printDetail(const Article& a) {
        cout << "\n  Article #" << a.id << "\n"
             << "  Title : " << a.title << "\n"
             << "  Source: " << a.source << "\n"
             << "  Author: " << (a.author.empty() ? "(none)" : a.author) << "\n"
             << "  Date  : " << (a.date.empty() ? "(none)" : a.date) << "\n"
             << "  Text  : " << a.text << "\n"
             << "  Score : " << a.score << "  ->  " << a.label << "\n";
        printFlags(a.id, "  ");
        const LinkedList<int>* nb = related.neighbors(a.id);
        if (nb && nb->size() > 0) {
            cout << "  Related: ";
            nb->display(cout, ", ");
            cout << "\n";
        }
    }

    void printAnalysis(const Article& a, const AnalysisResult& res) {
        cout << "\n  Analysis of #" << a.id << ": " << clip(a.title, 60) << "\n  Start score: 100\n";
        res.breakdown.forEach([&](const string& line) { cout << "    " << line << "\n"; });
        cout << "  Final score: " << a.score << "  ->  " << a.label << "\n";
        if (res.flags.size() > 0) cout << "  Flags: " << joinList(res.flags) << "\n";
        else cout << "  Flags: none\n";
    }

    bool askArticleId(const string& prompt, int& id) {
        int v;
        if (!readInt(prompt, 1, 99999999, v)) return false;
        id = v;
        if (!articles.contains(id)) {
            cout << "  No article with ID " << id << ".\n";
            return false;
        }
        return true;
    }

    // Looks for data/ in the current folder and a few folders above it
    // (Visual Studio runs the exe from out/build/x64-Debug).
    bool findDataDir(const string& explicitDir, string& found) {
        LinkedList<string> candidates;
        if (!explicitDir.empty()) {
            string d = explicitDir;
            if (d.back() != '/' && d.back() != '\\') d += "/";
            candidates.insertBack(d);
        }
        const char* prefixes[] = {"", "../", "../../", "../../../", "../../../../", "../../../../../"};
        for (size_t i = 0; i < arrayLen(prefixes); ++i) candidates.insertBack(prefixes[i]);
        for (LinkedList<string>::Node* p = candidates.getHead(); p != nullptr; p = p->next) {
            ifstream test(p->data + "data/articles.txt");
            if (test) {
                found = p->data;
                return true;
            }
        }
        return false;
    }

public:
    // ---------- start-up ----------
    bool load(const string& explicitDir) {
        if (!findDataDir(explicitDir, baseDir)) {
            cout << "Could not find data/articles.txt.\n"
                 << "Run the program from the project folder, or pass the project folder as an argument:\n"
                 << "    TruthLens.exe \"C:\\path\\to\\TruthLens\"\n";
            return false;
        }
        if (!loadSources(baseDir + "data/sources.txt"))
            cout << "Warning: data/sources.txt not found - every source will count as unknown.\n";

        LinkedList<int> ids;
        loadArticles(baseDir + "data/articles.txt", ids);

        // score everything only after the whole graph exists (corroboration needs it)
        for (LinkedList<int>::Node* p = ids.getHead(); p != nullptr; p = p->next) analyze(p->data, false, "");

        cout << "Loaded " << articles.size() << " articles, " << reputation.size() << " sources, "
             << keywordIndex.size() << " keywords, " << related.edgeCount() << " links between stories.\n";
        return true;
    }

    // ---------- 1. add article ----------
    void addArticle() {
        cout << "\n-- Add article (leave Title empty to cancel) --\n";
        Article a;
        a.title = trim(readLine("Title : "));
        if (a.title.empty()) { cout << "  Cancelled.\n"; return; }
        a.source = normalizeDomain(readLine("Source domain (e.g. dawn.com): "));
        if (a.source.empty()) { cout << "  Source is required. Cancelled.\n"; return; }
        a.author = trim(readLine("Author (optional): "));
        a.date = trim(readLine("Date YYYY-MM-DD (optional): "));
        a.text = trim(readLine("Text  : "));
        if (g_eof) return;
        for (size_t i = 0; i < a.title.size(); ++i) if (a.title[i] == '|') a.title[i] = '/';

        a.id = nextId;
        registerArticle(a);
        AnalysisResult res = analyze(a.id, true, "Added");

        Article* stored = articles.search(a.id);
        cout << "\n  Added as article #" << a.id << ".\n";
        printAnalysis(*stored, res);

        // the new article may corroborate its neighbours -> refresh their scores quietly
        const LinkedList<int>* nb = related.neighbors(a.id);
        if (nb && nb->size() > 0) {
            LinkedList<int> copy = *nb;
            copy.forEach([&](int nid) { analyze(nid, false, ""); });
            cout << "  Linked to " << copy.size() << " related article(s); their scores were refreshed.\n";
        }
        if (!reputation.contains(stored->source))
            cout << "  Note: '" << stored->source << "' is not in data/sources.txt, so it was treated as an unknown source.\n";
    }

    // ---------- 2. analyze article ----------
    void analyzeArticle() {
        int id;
        if (!askArticleId("Article ID to analyze: ", id)) return;
        AnalysisResult res = analyze(id, true, "Analyzed");
        printAnalysis(*articles.search(id), res);
    }

    // ---------- 3. search ----------
    void search() {
        string q = trim(readLine("Enter article ID or a keyword: "));
        if (q.empty()) return;
        if (allDigits(q) && q.size() < 9) {
            const Article* a = articles.search(stoi(q));
            if (a) printDetail(*a);
            else cout << "  No article with ID " << q << ".\n";
            return;
        }
        size_t sp = q.find(' ');
        if (sp != string::npos) {
            q = q.substr(0, sp);
            cout << "  (using the first word only: '" << q << "')\n";
        }
        q = toLower(q);
        const LinkedList<int>* ids = keywordIndex.search(q);
        if (ids == nullptr) {
            cout << "  No article contains '" << q << "'";
            if ((int)q.size() < MIN_KEYWORD_LEN)
                cout << " (only words with " << MIN_KEYWORD_LEN << "+ letters are indexed)";
            cout << ".\n";
            return;
        }
        cout << "  " << ids->size() << " article(s) contain '" << q << "':\n";
        ids->forEach([&](int id) { printLineById(id); });
    }

    // ---------- 4. list by credibility (BST in-order) ----------
    void listByCredibility() {
        LinkedList<int> ids;
        scoreTree.inorder(ids);
        cout << "\n  All articles by credibility (lowest -> highest), " << ids.size() << " total:\n";
        ids.forEach([&](int id) { printLineById(id); });
    }

    // ---------- 5. score range search (BST) ----------
    void rangeSearch() {
        int lo, hi;
        if (!readInt("Minimum score (0-100): ", 0, 100, lo)) return;
        if (!readInt("Maximum score (0-100): ", 0, 100, hi)) return;
        if (lo > hi) { int t = lo; lo = hi; hi = t; }
        LinkedList<int> ids;
        scoreTree.rangeSearch(lo, hi, ids);
        cout << "\n  " << ids.size() << " article(s) with score " << lo << " to " << hi << ":\n";
        ids.forEach([&](int id) { printLineById(id); });
    }

    // ---------- 6. related stories (BFS) ----------
    void relatedStories() {
        int id;
        if (!askArticleId("Article ID: ", id)) return;
        LinkedList<int> order, dist;
        related.bfs(id, order, dist);
        cout << "\n  Related stories for:\n";
        printLineById(id);
        if (order.size() <= 1) {
            cout << "  No related stories found.\n";
            return;
        }
        int lastHop = 0;
        LinkedList<int>::Node* o = order.getHead();
        LinkedList<int>::Node* d = dist.getHead();
        for (; o != nullptr; o = o->next, d = d->next) {
            if (d->data == 0) continue;                       // skip the start article itself
            if (d->data != lastHop) {
                lastHop = d->data;
                cout << (lastHop == 1 ? "  Directly related (1 hop):\n"
                                      : "  Indirectly related (" + to_string(lastHop) + " hops):\n");
            }
            printLineById(o->data);
        }
    }

    // ---------- 7. story clusters (DFS) ----------
    void storyClusters() {
        LinkedList<LinkedList<int>> comps;
        related.connectedComponents(comps);
        int clusterNo = 0;
        LinkedList<int> standalone;
        comps.forEach([&](const LinkedList<int>& comp) {
            if (comp.size() < 2) {
                standalone.insertBack(comp.getHead()->data);
                return;
            }
            int sum = 0;
            comp.forEach([&](int id) { sum += articles.search(id)->score; });
            cout << "\n  Cluster " << ++clusterNo << " (" << comp.size() << " articles, average score "
                 << sum / comp.size() << "):\n";
            comp.forEach([&](int id) { printLineById(id); });
        });
        if (clusterNo == 0) cout << "\n  No clusters found (no two articles are related).\n";
        if (standalone.size() > 0) {
            cout << "\n  Standalone articles (no related story): ";
            standalone.display(cout, ", ");
            cout << "\n";
        }
    }

    // ---------- 8. top K suspicious (min-heap) ----------
    void topSuspicious() {
        int k;
        if (!readInt("How many (K)? ", 1, 1000, k)) return;
        MinHeap heap;
        for (int id = 1; id < nextId; ++id) {
            const Article* a = articles.search(id);
            if (a) heap.insert(a->score, a->id);
        }
        if (k > heap.size()) k = heap.size();
        cout << "\n  Top " << k << " most suspicious article(s):\n";
        HeapItem item;
        for (int i = 1; i <= k && heap.extractMin(item); ++i) {
            cout << "  " << i << ".";
            printLineById(item.id);
            printFlags(item.id, "        ");
        }
    }

    // ---------- 9. flagged articles ----------
    void flaggedArticles() {
        LinkedList<int> ids;
        scoreTree.rangeSearch(0, CREDIBLE_MIN - 1, ids);      // everything that is not "Credible"
        cout << "\n  Flagged articles (score below " << CREDIBLE_MIN << "): " << ids.size() << "\n";
        ids.forEach([&](int id) {
            printLineById(id);
            printFlags(id, "        ");
        });
        if (ids.empty()) cout << "  None - every article is credible.\n";
    }

    // ---------- 10. analysis history ----------
    void analysisHistory() {
        if (history.empty()) {
            cout << "\n  No analyses yet this session (use option 1 or 2). Newest are shown first.\n";
            return;
        }
        cout << "\n  Analysis history (newest first):\n";
        history.forEach([&](const HistoryEntry& h) {
            cout << "  #" << setw(2) << h.seq << " " << left << setw(9) << h.action << right << "[" << setw(2) << h.id
                 << "] " << clip(h.title, 38) << "  ->  " << h.score << " " << h.label << "\n";
        });
    }
};

// ======================= menu =======================
int main(int argc, char** argv) {
    TruthLens app;
    if (!app.load(argc > 1 ? string(argv[1]) : string())) return 1;

    int choice = -1;
    do {
        cout << "\n=== TruthLens ===\n"
             << "1. Add article\n2. Analyze article\n3. Search by ID/keyword\n"
             << "4. List by credibility (BST)\n5. Score range search (BST)\n"
             << "6. Related stories (Graph BFS)\n7. Story clusters (Graph DFS)\n"
             << "8. Top K suspicious (Heap)\n9. Flagged articles\n10. Analysis history\n"
             << "0. Exit\n";
        if (!readInt("> ", 0, 10, choice)) break;
        switch (choice) {
            case 1:  app.addArticle(); break;
            case 2:  app.analyzeArticle(); break;
            case 3:  app.search(); break;
            case 4:  app.listByCredibility(); break;
            case 5:  app.rangeSearch(); break;
            case 6:  app.relatedStories(); break;
            case 7:  app.storyClusters(); break;
            case 8:  app.topSuspicious(); break;
            case 9:  app.flaggedArticles(); break;
            case 10: app.analysisHistory(); break;
            default: break;
        }
    } while (choice != 0 && !g_eof);

    cout << "Goodbye!\n";
    return 0;
}
