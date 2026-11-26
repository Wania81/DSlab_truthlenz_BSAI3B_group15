# TruthLens – AI-Based News Credibility Analyzer

**Data Structures Lab Project**  
NUCES Faisalabad-Chiniot Campus

## About the Project

TruthLens is a C++ project that checks news articles and gives them a credibility score between 0 and 100. It helps us identify whether a news article looks trustworthy or if it might contain misleading information.

The project uses some predefined rules to calculate the score. It does not use machine learning or any trained AI model. It checks different things in an article, such as its source, title, author, date, emotional words and citations.

Based on the final score, the article gets one of these labels:

| Score | Result |
|---|---|
| 70 - 100 | Credible |
| 40 - 69 | Questionable |
| Below 40 | Misleading |

## Team Members

- Seemal Atif
- Wania Hassan

## Data Structures Used

In this project, we have used five different data structures. We implemented them ourselves instead of using STL containers in C++.

| Data Structure | File Name | Purpose |
|---|---|---|
| Linked List | `LinkedList.h` | Used in different parts of the project, like hash table chaining, flags and analysis history. It is also used in the graph and BFS queue. |
| Hash Table | `HashTable.h` | Used to search articles by ID, store source reputation, keywords and article flags. |
| BST | `BST.h` | Stores articles according to their credibility scores and helps in searching articles within a score range. |
| Min-Heap | `MinHeap.h` | Helps us find the articles with the lowest credibility scores. |
| Graph | `Graph.h` | Used to find related news articles through BFS and story groups through DFS. |

## Project Files

Our project contains the following files and folders:

```text
TruthLens/
  main.cpp          Main file containing the menu
  Analyzer.h        Contains the rules used for scoring
  Article.h         Contains the Article structure
  LinkedList.h      Linked List implementation
  HashTable.h       Hash Table implementation
  BST.h             Binary Search Tree implementation
  MinHeap.h         Min Heap implementation
  Graph.h           Graph implementation
  CMakeLists.txt    File used for building the project
  data/
    articles.txt    Contains news articles
    sources.txt     Contains source reputation
  poster.html       Project poster
  SUBMISSION.txt    Submission details
```

## How Does the Project Work?

When we run the program, it first loads the source reputation from `sources.txt` and news articles from `articles.txt`.

After loading the articles, the program extracts important keywords from their text. These keywords are stored in a hash table so they can be searched later.

The program also checks which articles are related to each other. If two articles have at least 3 common keywords, they are connected in the graph.

Once all articles are loaded and the graph is ready, the program calculates the credibility score of every article and stores the articles in the BST according to their scores.

After this process, we can use the menu to perform different operations.

## How Is the Credibility Score Calculated?

Every article starts with a score of 100. The program then checks different conditions and increases or decreases the score.

Some points are deducted if the article contains suspicious signs. Points can