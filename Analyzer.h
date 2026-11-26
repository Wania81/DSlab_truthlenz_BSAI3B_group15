#pragma once
// Person 2: credibility scoring.
// scoreArticle() starts from 100 and subtracts / adds points for each indicator.
// All weights are named constants below, so they are easy to tune.
#include <cctype>
#include <string>
#include "Article.h"
#include "LinkedList.h"
using namespace std;

// ---------------- tunable weights ----------------
const int SOURCE_WEIGHT_PCT      = 40;  // penalty = (100 - reputation) * 40 / 100
const int DEFAULT_REPUTATION     = 50;  // used when the source is not in sources.txt
const int UNRELIABLE_REPUTATION  = 40;  // below this the source is flagged "Unreliable"
const int TRUSTED_REPUTATION     = 60;  // a related article from a source >= this counts as corroboration

const int CLICKBAIT_PENALTY      = 8;   // per distinct clickbait phrase
const int CLICKBAIT_CAP          = 24;
const int CAPS_EXCL_PENALTY      = 8;   // too many ALL-CAPS words or '!' marks
const int CAPS_WORDS_LIMIT       = 3;   // this many ALL-CAPS words (3+ letters) triggers it
const int EXCLAMATION_LIMIT      = 3;   // this many '!' characters triggers it
const int EMOTIONAL_PENALTY      = 4;   // per distinct emotional word
const int EMOTIONAL_CAP          = 12;
const int MISSING_AUTHOR_PENALTY = 8; 
const int MISSING_DATE_PENALTY   = 5;
const int NO_CITATION_PENALTY    = 12;  // 0 citation phrases
const int FEW_CITATION_PENALTY   = 5;   // exactly 1 citation phrase
const int CORROBORATION_BONUS    = 5;   // per related article from a different, trusted source
const int CORROBORATION_CAP      = 10;

// ---------------- labels ----------------
const int CREDIBLE_MIN     = 70;        // 70-100 Credible
const int QUESTIONABLE_MIN = 40;        // 40-69 Questionable, below 40 Misleading

// ---------------- keyword / graph settings ----------------
const int MIN_KEYWORD_LEN     = 4;      // shorter words are ignored
const int MIN_SHARED_KEYWORDS = 3;      // edge in the graph if two articles share >= this many keywords

// ---------------- word lists ----------------
static const char* const CLICKBAIT_PHRASES[] = {
    "shocking", "miracle", "you won't believe", "doctors hate", "secret", "the truth",
    "hiding", "hides", "banned", "wake up", "don't want you", "share before", "buy now",
    "cures everything", "overnight", "click here", "must see", "unbelievable"
};
static const char* const EMOTIONAL_WORDS[] = {
    "shocked", "terrifying", "horrifying", "outrage", "furious", "devastating",
    "stunned", "panic", "nightmare", "scary", "disgusting", "insane"
};
static const char* const CITATION_PHRASES[] = {
    "according to", "reported", "announced", "quoted", "peer-reviewed", "researchers",
    "analysts", "spokesperson", "press release", "officials"
};
static const char* const STOPWORDS[] = {
    "that", "this", "with", "from", "have", "will", "been", "were", "they", "their", "what",
    "when", "which", "about", "after", "before", "would", "there", "them", "than", "then",
    "into", "over", "also", "said", "says", "more", "most", "some", "such", "only", "just",
    "your", "does", "here", "week", "according", "reported", "reports", "officials",
    "announced", "statement", "analysts", "researchers", "this", "year", "later", "while"
};

template <typename T, size_t N>
constexpr size_t arrayLen(const T (&)[N]) { return N; }

// ---------------- small text helpers ----------------
inline string toLower(const string& s) {
    string r = s;
    for (size_t i = 0; i < r.size(); ++i) r[i] = (char)tolower((unsigned char)r[i]);
    return r;
}

inline bool containsPhrase(const string& lowerText, const string& phrase) {
    return lowerText.find(phrase) != string::npos;
}

inline bool isStopword(const string& w) {
    for (size_t i = 0; i < arrayLen(STOPWORDS); ++i)
        if (w == STOPWORDS[i]) return true;
    return false;
}

// Distinct meaningful words of title + text (lower-case, letters only).
// Used for the keyword index (hash table) and for building graph edges.
inline void extractKeywords(const Article& a, LinkedList<string>& out) {
    out.clear();
    string text = toLower(a.title + " " + a.text);
    string word;
    for (size_t i = 0; i <= text.size(); ++i) {
        unsigned char c = (i < text.size()) ? (unsigned char)text[i] : (unsigned char)' ';
        if (isalpha(c)) {
            word.push_back((char)c);
        } else {
            if ((int)word.size() >= MIN_KEYWORD_LEN && !isStopword(word) && !out.contains(word))
                out.insertBack(word);
            word.clear();
        }
    }
}

inline string labelFor(int score) {
    if (score >= CREDIBLE_MIN) return "Credible";
    if (score >= QUESTIONABLE_MIN) return "Questionable";
    return "Misleading";
}

// Words written fully in capitals (3+ letters), e.g. SHOCKING.
inline int countAllCapsWords(const string& s) {
    int caps = 0;
    string word;
    for (size_t i = 0; i <= s.size(); ++i) {
        unsigned char c = (i < s.size()) ? (unsigned char)s[i] : (unsigned char)' ';
        if (isalpha(c)) {
            word.push_back((char)c);
        } else {
            if (word.size() >= 3) {
                bool allUpper = true;
                for (size_t k = 0; k < word.size(); ++k)
                    if (!isupper((unsigned char)word[k])) { allUpper = false; break; }
                if (allUpper) ++caps;
            }
            word.clear();
        }
    }
    return caps;
}

inline int countChar(const string& s, char ch) {
    int c = 0;
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] == ch) ++c;
    return c;
}

inline string joinList(const LinkedList<string>& list) {
    string r;
    LinkedList<string>::Node* p = list.getHead();
    for (; p != nullptr; p = p->next) {
        r += p->data;
        if (p->next != nullptr) r += ", ";
    }
    return r;
}

// ---------------- scoring ----------------
struct Evidence {
    int reputation = DEFAULT_REPUTATION;  // reputation of the article's source (0-100)
    bool sourceKnown = false;             // is the source listed in sources.txt?
    int corroborations = 0;               // related articles from other, trusted sources (from the graph)
};

struct AnalysisResult {
    LinkedList<string> flags;      // triggered warning indicators
    LinkedList<string> breakdown;  // every score change with its points (shown in "Analyze")
};

// Computes a.score and a.label and returns the indicators that were triggered.
inline AnalysisResult scoreArticle(Article& a, const Evidence& ev) {
    AnalysisResult res;
    int score = 100;

    auto apply = [&](int delta, const string& flag, const string& detail) {
        score += delta;
        if (!flag.empty()) res.flags.insertBack(flag);
        res.breakdown.insertBack(detail + " (" + (delta > 0 ? "+" : "") + to_string(delta) + ")");
    };

    const string lowerTitle = toLower(a.title);
    const string lowerAll = toLower(a.title + " " + a.text);
    const string rawAll = a.title + " " + a.text;

    // 1. Source reputation
    int srcPenalty = (100 - ev.reputation) * SOURCE_WEIGHT_PCT / 100;
    if (!ev.sourceKnown)
        apply(-srcPenalty, "Unknown source",
              "Source '" + a.source + "' not in reputation list (assumed " + to_string(ev.reputation) + "/100)");
    else if (ev.reputation < UNRELIABLE_REPUTATION)
        apply(-srcPenalty, "Unreliable source",
              "Source '" + a.source + "' has low reputation " + to_string(ev.reputation) + "/100");
    else
        apply(-srcPenalty, "",
              "Source '" + a.source + "' reputation " + to_string(ev.reputation) + "/100");

    // 2. Clickbait phrases
    LinkedList<string> hits;
    bool inTitle = false;
    for (size_t i = 0; i < arrayLen(CLICKBAIT_PHRASES); ++i) {
        string p = CLICKBAIT_PHRASES[i];
        if (containsPhrase(lowerAll, p)) {
            hits.insertBack(p);
            if (containsPhrase(lowerTitle, p)) inTitle = true;
        }
    }
    if (hits.size() > 0) {
        int pen = hits.size() * CLICKBAIT_PENALTY;
        if (pen > CLICKBAIT_CAP) pen = CLICKBAIT_CAP;
        apply(-pen, inTitle ? "Clickbait headline" : "Clickbait wording",
              "Clickbait phrases: " + joinList(hits));
    }

    // 3. ALL CAPS / exclamation marks
    int caps = countAllCapsWords(rawAll);
    int excl = countChar(rawAll, '!');
    if (caps >= CAPS_WORDS_LIMIT || excl >= EXCLAMATION_LIMIT)
        apply(-CAPS_EXCL_PENALTY, "Excessive caps/exclamation",
              "Shouting: " + to_string(caps) + " ALL-CAPS words, " + to_string(excl) + " '!' marks");

    // 4. Emotional language
    LinkedList<string> emo;
    for (size_t i = 0; i < arrayLen(EMOTIONAL_WORDS); ++i)
        if (containsPhrase(lowerAll, EMOTIONAL_WORDS[i])) emo.insertBack(EMOTIONAL_WORDS[i]);
    if (emo.size() > 0) {
        int pen = emo.size() * EMOTIONAL_PENALTY;
        if (pen > EMOTIONAL_CAP) pen = EMOTIONAL_CAP;
        apply(-pen, "Emotional language", "Emotional words: " + joinList(emo));
    }

    // 5. Missing metadata
    if (a.author.empty()) apply(-MISSING_AUTHOR_PENALTY, "Missing author", "No author given");
    if (a.date.empty())   apply(-MISSING_DATE_PENALTY, "Missing date", "No date given");

    // 6. Citations
    int cites = 0;
    for (size_t i = 0; i < arrayLen(CITATION_PHRASES); ++i)
        if (containsPhrase(lowerAll, CITATION_PHRASES[i])) ++cites;
    if (cites == 0)      apply(-NO_CITATION_PENALTY, "No citations", "No citation phrases found");
    else if (cites == 1) apply(-FEW_CITATION_PENALTY, "Few citations", "Only 1 citation phrase found");

    // 7. Corroboration (from the graph of related stories)
    if (ev.corroborations > 0) {
        int bonus = ev.corroborations * CORROBORATION_BONUS;
        if (bonus > CORROBORATION_CAP) bonus = CORROBORATION_CAP;
        apply(bonus, "",
              "Corroborated by " + to_string(ev.corroborations) + " related article(s) from trusted sources");
    }

    if (score < 0) score = 0;
    if (score > 100) score = 100;
    a.score = score;
    a.label = labelFor(score);
    return res;
}
