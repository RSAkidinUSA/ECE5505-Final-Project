///////////////////////////////////////////////////////////////////////
// 3-value Logic Simulator, written by Michael Hsiao, Modified by Ryan Burrow
//   Project 1, ECE 5505
///////////////////////////////////////////////////////////////////////

#include <cstdlib>
#include <sys/times.h>
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <ctime>
using namespace std;

#define HZ 100
#define RETURN '\n'
#define EOS '\0'
#define COMMA ','
#define SPACE ' '
#define TAB '\t'
#define COLON ':'
#define SEMICOLON ';'

#define SPLITMERGE 'M'

#define T_INPUT 1
#define T_OUTPUT 2
#define T_SIGNAL 3
#define T_MODULE 4
#define T_COMPONENT 5
#define T_EXIST 9
#define T_COMMENT 10
#define T_END 11

#define TABLESIZE 5000
#define MAXIO 5000
#define MAXMODULES 5000
#define MAXDFF 10560

#define GOOD 1
#define FAULTY 2
#define DONTCARE -1
#define ALLONES 0xffffffff

#define MAXlevels 10000
#define MAXIOS 5120
#define MAXFanout 10192
#define MAXFFS 40048
#define MAXGATES 100000
#define MAXevents 100000

#define TRUE 1
#define FALSE 0

#define EXCITED_1_LEVEL 1
#define POTENTIAL 2
#define LOW_DETECT 3
#define HIGH_DETECT 4
#define REDUNDANT 5

enum {
   JUNK,           /* 0 */
   T_input,        /* 1 */
   T_output,       /* 2 */
   T_xor,          /* 3 */
   T_xnor,         /* 4 */
   T_dff,          /* 5 */
   T_and,          /* 6 */
   T_nand,         /* 7 */
   T_or,           /* 8 */
   T_nor,          /* 9 */
   T_not,          /* 10 */
   T_buf,          /* 11 */
   T_tie1,         /* 12 */
   T_tie0,         /* 13 */
   T_tieX,         /* 14 */
   T_tieZ,         /* 15 */
   T_mux_2,        /* 16 */
   T_bus,          /* 17 */
   T_bus_gohigh,   /* 18 */
   T_bus_golow,    /* 19 */
   T_tristate,     /* 20 */
   T_tristateinv,  /* 21 */
   T_tristate1     /* 22 */
};


////////////////////////////////////////////////////////////////////////
// gateLevelCkt class
////////////////////////////////////////////////////////////////////////

/* Values xtree variable can have */
#define X_TREE_NA  0x0
#define X_TREE_ON  0x1
#define X_TREE_OFF 0x2

class gateLevelCkt {
    // circuit information
    int numgates;   // total number of gates (faulty included)
    int numFaultFreeGates;  // number of fault free gates
    int numpri;     // number of PIs
    int numout;     // number of POs
    int maxlevels;  // number of levels in gate level ckt
    int maxLevelSize;   // maximum number of gates in one given level
    int levelSize[MAXlevels];   // levelSize for each level
    int inputs[MAXIOS];
    int outputs[MAXIOS];
    int ff_list[MAXFFS];
    int *ffMap;
    unsigned char *gtype;   // gate type
    short *fanin;       // number of fanin, fanouts
    short *fanout;
    int *levelNum;      // level number of gate
    unsigned *po;
    int **inlist;       // fanin list
    int **fnlist;       // fanout list
    char *sched;        // scheduled on the wheel yet?
    unsigned int *value1;
    unsigned int *value2;   // value of gate
    unsigned int *xlevel;   // level of x
    unsigned int xlevelMAX;
    short **xtree;  // tree of xs for output - 0 is ignore, 1 is Xn, 2 is X!n
    int *xOnList;   // list of the first occurance of each X
    int *xOffList;  // list of the first occurance of each X!
    int **predOfSuccInput;      // predecessor of successor input-pin list
    int **succOfPredOutput;     // successor of predecessor output-pin list

    // for simulator
    int **levelEvents;  // event list for each level in the circuit
    int *levelLen;  // evenlist length
    int numlevels;  // total number of levels in wheel
    int currLevel;  // current level
    int *activation;    // activation list for the current level in circuit
    int actLen;     // length of the activation list
    int *actFFList; // activation list for the FF's
    int actFFLen;   // length of the actFFList
public:
    int numff;      // number of FF's
    unsigned int *RESET_FF1;    // value of reset ffs read from *.initState
    unsigned int *RESET_FF2;    // value of reset ffs read from *.initState

    gateLevelCkt(string);   // constructor
    void setFaninoutMatrix();   // builds the fanin-out map matrix

    void applyVector(char *);   // apply input vector

    // simulator information
    void setupWheel(int, int);
    void insertEvent(int, int);
    int retrieveEvent();
    int setValue(int gate, char value); // change a value on a gate and add successors to event wheel
    void goodsim();     // logic sim (no faults inserted)

    void setTieEvents();    // inject events from tied nodes

    void observeOutputs();  // print the fault-free outputs
    void observeXTrees();  // observe the X chains
    void printXTree(int gate);
    void printGoodSig(ofstream, int);   // print the fault-free outputs to *.sig

    char *goodState;        // good state (without scan)
};

//////////////////////////////////////////////////////////////////////////
// Global Variables

char vector[5120];
gateLevelCkt *circuit;
int OBSERVE, XTREE, INTERACT;
int vecNum=0;
int numTieNodes;
int TIES[512];


//////////////////////////////////////////////////////////////////////////
// Functions start here


//////////////////////////////////////////////////////////////////////////
// returns 1 if a regular vector, returns 2 if a scan
//////////////////////////////////////////////////////////////////////////
int getVector(ifstream &inFile, int vecSize) {
    int i;
    char thisChar;

    inFile >> thisChar;
    while ((thisChar == SPACE) || (thisChar == RETURN)) {
        inFile >> thisChar;
    }

    vector[0] = thisChar;
    if (vector[0] == 'E') {
        return (0);
    }

    for (i=1; i<vecSize; i++) {
        inFile >> vector[i];
    }
    vector[i] = EOS;

    return(1);
}

// logicSimFromFile() - logic simulates all vectors in vecFile on the circuit
int logicSimFromFile(ifstream &vecFile, int vecWidth) {
    int moreVec;
    int skipnum = 0;
    bool quit = false;

    moreVec = 1;
    while (moreVec && !quit) {
        moreVec = getVector(vecFile, vecWidth);
        if (moreVec == 1) {
            cout << "vector #" << vecNum << ":\n";
            circuit->applyVector(vector);
            circuit->goodsim();      // simulate the vector
            if (OBSERVE) {
                circuit->observeOutputs();
            }
            if (XTREE) {
                circuit->observeXTrees();
            }
            if (INTERACT && !quit) {
                char cmd, val;
                int temp, retval;
                bool skip = false;
                if (skipnum >= 1) {
                    skip = true;
                    skipnum--;
                    cout << "Skipping interactive mode for vector #" << vecNum << endl;
                } else {
                    cout << "Entering interactive mode for vector #" << vecNum << endl;
                    cout << "Enter a command or h for help\n";
                }
                while (!skip) {
                    bool validVal = false;
                    cout << "cmd: ";
                    cin >> cmd;
                    switch (cmd) {
                        case 'g':
                        case 'G':
                            cin >> temp;
                            while (!validVal) {
                                cout << "What would you like to set gate #" << temp << " to? (1,0,X): ";
                                cin >> val;
                                validVal = true;
                                switch (val) {
                                    case 'X':
                                        val = 'x';
                                    case 'x':
                                    case '1':
                                    case '0':
                                        /* Set the value here */
                                        retval = circuit->setValue(temp, val);
                                        if (retval > 0) {
                                            cout << "Set gate #" << temp << " to " << val << endl;
                                            /* Schedule successors */
                                            circuit->goodsim();

                                            if (OBSERVE) {
                                                cout << "New output:\n";
                                                circuit->observeOutputs();
                                            }
                                            if (XTREE) {
                                                cout << "New XTrees:\n";
                                                circuit->observeXTrees();
                                            }
                                        } else if (retval == 0) {
                                            cout << "Gate value did not change\n";
                                        } else {
                                            cout << "Invalid gate\n";
                                        }
                                        continue;
                                    case 'q':
                                    case 'Q':
                                        continue;
                                    default:
                                        validVal = false;
                                        continue;
                                }
                            }
                            continue;
                        case 'r':
                        case 'R':
                            cout << "Applying vector #" << vecNum << endl;
                            circuit->applyVector(vector);
                            circuit->goodsim();
                            if (OBSERVE) {
                                cout << "Output:\n";
                                circuit->observeOutputs();
                            }
                            if (XTREE) {
                                cout << "XTrees:\n";
                                circuit->observeXTrees();
                            } 
                            continue;
                        case 's':
                        case 'S':
                            cin >> skipnum;
                        case 'c':
                        case 'C':
                            skip = true;
                            continue;
                        case 'q':
                        case 'Q':
                            quit = true;
                            skip = true;
                            continue;
                        case 'x':
                        case 'X':
                            if (XTREE) {
                                cin >> temp;
                                cout << "X-Tree at gate " << temp << ":\n";
                                circuit->printXTree(temp);
                                continue;
                            } else {
                                cout << "-x option must be enabled to use this command\n";
                            }
                        default:
                            cout << "Invalid command: " << cmd << endl;
                        case 'h':
                        case 'H':
                            cout << "List of commands is as follows:\n";
                            cout << "\th - help\n";
                            cout << "\tq - quit\n";
                            cout << "\tr - reset the circuit to the original applied vector\n";
                            cout << "\tc - continue to the next vector\n";
                            cout << "\ts# - skip this and # following vectors (s0 is equivalent to c)\n";
                            cout << "\tg# - set gate #'s value\n";
                            if (XTREE) {
                                cout << "\tx# - view X tree for gate #\n";
                            }
                            continue;
                    }
                }
            }
            vecNum++;
        }  // if (moreVec == 1)
    }   // while (getVector...)

    return (vecNum);
}


// main()
main(int argc, char *argv[]) {
    ifstream vecFile;
    string cktName, vecName;
    int totalNumVec, vecWidth, i;
    int nameIndex;
    double ut;
    clock_t start, end;

    start = clock();

    if ((argc != 2) && (argc != 3)) {
        cerr << "Usage: " << argv[0] << "[-xio] <ckt>\n";
        cerr << "The -x option is to view the X chain for each X output.\n";
        cerr << "The -i option enables interactive mode, where the user can flip gate values\n";
        cerr << "and -o option is to OBSERVE fault-free outputs.\n";
        cerr << " Example: " << argv[0] << " s27\n";
        cerr << " Example2: " << argv[0] << " -o s27\n";
        cerr << " Example3: " << argv[0] << " -io s27\n";
        exit(-1);
    }

    if (argc == 3) {
        i = 1;
        nameIndex = 2;
        OBSERVE = 0;
        XTREE = 0;
        while (argv[1][i] != EOS) {
            switch (argv[1][i]) {
            case 'o':
                OBSERVE = 1;
                break;
            case 'x':
                XTREE = 1;
                break;
            case 'i':
                INTERACT = 1;
                break;
            default:
                cerr << "Invalid option: " << argv[1] << "\n";
                cerr << "Usage: " << argv[0] << " [-xio] <ckt>\n";
                cerr << "The -x option is to view the X trees for each output.\n";
                cerr << "The -i option enables interactive mode\n";
                cerr << "With both -x and -i enabled the user can see X trees at any gate\n";
                cerr << "and -o option is to OBSERVE fault-free outputs.\n";
                exit(-1);
                break;
            }   // switch
            i++;
        }   // while
    } else {   // no option
        nameIndex = 1;
        OBSERVE = 0;
        XTREE = 0;
        INTERACT = 0;
    }

    cktName = argv[nameIndex];
    vecName = argv[nameIndex];
    vecName += ".vec";

    circuit = new gateLevelCkt(cktName);

    vecFile.open(vecName.c_str(), ios::in);
    if (vecFile == NULL) {
        cerr << "Can't open " << vecName << "\n";
        exit(-1);
    }

    vecFile >> vecWidth;

    circuit->setTieEvents();
    totalNumVec = logicSimFromFile(vecFile, vecWidth);
    vecFile.close();

    // output results
    end = clock();
    ut = (double) (end-start);
    cout << "Number of vectors: " << totalNumVec << "\n";
    cout << "Number of clock cycles elapsed: " << (double) ut << "\n";
}


////////////////////////////////////////////////////////////////////////
inline void gateLevelCkt::insertEvent(int levelN, int gateN) {
    levelEvents[levelN][levelLen[levelN]] = gateN;
    levelLen[levelN]++;
    /* reset the xtree for the gate */
    memset(xtree[gateN], X_TREE_NA, sizeof(*xtree[gateN]) * (numgates+64));
}

////////////////////////////////////////////////////////////////////////
// gateLevelCkt class
////////////////////////////////////////////////////////////////////////

// constructor: reads in the *.lev file for the gate-level ckt
gateLevelCkt::gateLevelCkt(string cktName) {
    ifstream yyin;
    string fName;
    int count;
    char c;
    int netnum, junk;
    int f1, f2, f3;
    int levelSize[MAXlevels];

    fName = cktName + ".lev";
    yyin.open(fName.c_str(), ios::in);
    if (yyin == NULL) {
        cerr << "Can't open .lev file\n";
        exit(-1);
    }

    numpri = numgates = numout = maxlevels = numff = 0;
    maxLevelSize = 32;
    for (int i = 0; i < MAXlevels; i++) {
        levelSize[i] = 0;
    }

    yyin >>  count; // number of gates
    yyin >> junk;

    // allocate space for gates
    gtype = new unsigned char[count+64];
    fanin = new short[count+64];
    fanout = new short[count+64];
    levelNum = new int[count+64];
    po = new unsigned[count+64];
    inlist = new int * [count+64];
    fnlist = new int * [count+64];
    sched = new char[count+64];
    value1 = new unsigned int[count+64];
    value2 = new unsigned int[count+64];
    xlevel = new unsigned int[count+64];
    xlevelMAX = 1;

    // now read in the circuit
    numTieNodes = 0;
    for (int i = 1; i < count; i++) {
        yyin >> netnum;
        yyin >> f1;
        yyin >> f2;
        yyin >> f3;

        numgates++;
        gtype[netnum] = (unsigned char) f1;
        f2 = (int) f2;
        levelNum[netnum] = f2;
        levelSize[f2]++;

        if (f2 >= (maxlevels)) {
            maxlevels = f2 + 5;
        }
        if (maxlevels > MAXlevels) {
            cerr << "MAXIMUM level (" << maxlevels << ") exceeded.\n";
            exit(-1);
        }

        fanin[netnum] = (int) f3;
        if (f3 > MAXFanout) {
            cerr << "Fanin count (" << f3 << " exceeded\n";
        }

        if (gtype[netnum] == T_input) {
            inputs[numpri] = netnum;
            numpri++;
        }
        if (gtype[netnum] == T_dff) {
            if (numff >= (MAXFFS-1)) {
                cerr << "The circuit has more than " << MAXFFS -1 << " FFs\n";
                exit(-1);
            }
            ff_list[numff] = netnum;
            numff++;
        }

        sched[netnum] = 0;

        // now read in the faninlist
        inlist[netnum] = new int[fanin[netnum]];
        for (int j = 0; j < fanin[netnum]; j++) {
            yyin >> f1;
            inlist[netnum][j] = (int) f1;
        }

        for (int j = 0; j < fanin[netnum]; j++) {   // followed by close to samethings
            yyin >> junk;
        }

        // read in the fanout list
        yyin >> f1;
        fanout[netnum] = (int) f1;

        if (gtype[netnum] == T_output) {
            po[netnum] = TRUE;
            outputs[numout] = netnum;
            numout++;
        } else {
            po[netnum] = 0;
        }

        if (fanout[netnum] > MAXFanout) {
            cerr << "Fanout count (" << fanout[netnum] << ") exceeded\n";
        }

        fnlist[netnum] = new int[fanout[netnum]];
        for (int j = 0; j < fanout[netnum]; j++) {
            yyin >> f1;
            fnlist[netnum][j] = (int) f1;
        }

        if (gtype[netnum] == T_tie1) {
            TIES[numTieNodes] = netnum;
            numTieNodes++;
            if (numTieNodes > 511) {
                cerr, "Can't handle more than 512 tied nodes\n";
                exit(-1);
            }
            value1[netnum] = ALLONES;
            value2[netnum] = ALLONES;
            xlevel[netnum] = 0;
        } else if (gtype[netnum] == T_tie0) {
            TIES[numTieNodes] = netnum;
            numTieNodes++;
            if (numTieNodes > 511) {
                cerr << "Can't handle more than 512 tied nodes\n";
                exit(-1);
            }
            value1[netnum] = 0;
            value2[netnum] = 0;
            xlevel[netnum] = 0;
        } else {
            // assign all values to unknown
            value1[netnum] = 0;
            value2[netnum] = ALLONES;
            xlevel[netnum] = 0;
        }

        // read in and discard the observability values
        yyin >> junk;
        yyin >> c;    // some character here
        yyin >> junk;
        yyin >> junk;

    }   // for (i...)
    yyin.close();
    numgates++;
    numFaultFreeGates = numgates;

    // now compute the maximum width of the level
    for (int i = 0; i < maxlevels; i++) {
        if (levelSize[i] > maxLevelSize) {
            maxLevelSize = levelSize[i] + 1;
        }
    }

    // allocate space for the faulty gates
    for (int i = numgates; i < numgates + 64; i+=2) {
        inlist[i] = new int[2];
        fnlist[i] = new int[MAXFanout];
        po[i] = 0;
        fanin[i] = 2;
        inlist[i][0] = i+1;
        sched[i] = 0;
    }

    cout << "Successfully read in circuit:\n";
    cout << "\t" << numpri << " PIs.\n";
    cout << "\t" << numout << " POs.\n";
    cout << "\t" << numff << " Dffs.\n";
    cout << "\t" << numFaultFreeGates << " total number of gates\n";
    cout << "\t" << maxlevels / 5 << " levels in the circuit.\n";
    goodState = new char[numff];
    ffMap = new int[numgates];
    // get the ffMap
    for (int i = 0; i < numff; i++) {
        ffMap[ff_list[i]] = i;
        goodState[i] = 'X';
    }

    setupWheel(maxlevels, maxLevelSize);
    setFaninoutMatrix();

    xtree = new short * [count+64];
    xOnList = new int[count+64];
    xOffList = new int[count+64];
    for (int i = 0; i < numgates + 64; i++) {
        xtree[i] = new short[numgates+64]();
    }
}

////////////////////////////////////////////////////////////////////////
// setFaninoutMatrix()
//  This function builds the matrix of succOfPredOutput and 
// predOfSuccInput.
////////////////////////////////////////////////////////////////////////

void gateLevelCkt::setFaninoutMatrix() {
    int k;
    int predecessor, successor;
    int checked[MAXFanout];
    int checkID;    // needed for gates with fanouts to SAME gate
    int prevSucc, found;

    predOfSuccInput = new int *[numgates+64];
    succOfPredOutput = new int *[numgates+64];
    for (int i = 0; i < MAXFanout; i++) {
        checked[i] = 0;
    }
    checkID = 1;

    prevSucc = -1;
    for (int i = 1; i < numgates; i++) {
        predOfSuccInput[i] = new int [fanout[i]];
        succOfPredOutput[i] = new int [fanin[i]];

        for (int j = 0; j < fanout[i]; j++) {
            if (prevSucc != fnlist[i][j]) {
                checkID++;
            }
            prevSucc = fnlist[i][j];

            successor = fnlist[i][j];
            k = found = 0;
            while ((k < fanin[successor]) && (!found)) {
                if ((inlist[successor][k] == i) && (checked[k] != checkID)) {
                    predOfSuccInput[i][j] = k;
                    checked[k] = checkID;
                    found = 1;
                }
                k++;
            }
        }

        for (int j = 0; j < fanin[i]; j++) {
            if (prevSucc != inlist[i][j]) {
                checkID++;
            }
            prevSucc = inlist[i][j];

            predecessor = inlist[i][j];
            k = found = 0;
            while ((k < fanout[predecessor]) && (!found)) {
                if ((fnlist[predecessor][k] == i) && (checked[k] != checkID)) {
                    succOfPredOutput[i][j] = k;
                    checked[k] = checkID;
                    found=1;
                }
                k++;
            }
        }
    }

    for (int i = numgates; i < numgates + 64; i+=2) {
        predOfSuccInput[i] = new int[MAXFanout];
        succOfPredOutput[i] = new int[MAXFanout];
    }
}

////////////////////////////////////////////////////////////////////////
// setTieEvents()
//  This function set up the events for tied nodes
////////////////////////////////////////////////////////////////////////

void gateLevelCkt::setTieEvents() {
    int predecessor, successor;

    for (int i = 0; i < numTieNodes; i++) {
        // different from previous time frame, place in wheel
        for (int j = 0; j < fanout[TIES[i]]; j++) {
            successor = fnlist[TIES[i]][j];
            if (sched[successor] == 0) {
                insertEvent(levelNum[successor], successor);
                sched[successor] = 1;
            }
        }
    }   // for (i...)
}

////////////////////////////////////////////////////////////////////////
// setValue()
//  This function sets the value of a gate, adding to the event wheel if necessary
//  The return value is 1 if the gate changes value, 0 if not, -1 if invalid gate
////////////////////////////////////////////////////////////////////////
int gateLevelCkt::setValue(int gate, char value) {
    unsigned int origVal1, origVal2;
    char origBit;
    int successor;
    int count = 0;

    if (gate > numgates) {
        return -1;
    }

    origVal1 = value1[gate] & 1;
    origVal2 = value2[gate] & 1;
    if ((origVal1 == 1) && (origVal2 == 1)) {
        origBit = '1';
    } else if ((origVal1 == 0) && (origVal2 == 0)) {
        origBit = '0';
    } else {
        origBit = 'x';
    }
    if (origBit == value && origBit != 'x') {
        return 0;
    } else if (origBit == 'x' && value == 'x') {
        cout << "The old value was X, the new value is too, so I'll just propagate it for you...\n";
    } else {
        memset(xtree[gate], X_TREE_NA, sizeof(*xtree[gate]) * (numgates+64));
        switch (value) {
            case '0':
                value1[gate] = 0;
                value2[gate] = 0;
                xlevel[gate] = 0;
                break;
            case '1':
                value1[gate] = ALLONES;
                value2[gate] = ALLONES;
                xlevel[gate] = 0;
                break;
            case 'x':
            case 'X':
                value1[gate] = 0;
                value2[gate] = ALLONES;
                xlevel[gate] = ++xlevelMAX;
                xtree[gate][xlevelMAX] |= X_TREE_ON;
                if (XTREE) {
                    xOnList[xlevelMAX] = gate;
                }
                break;
            default:
                cerr << value << ": error value passed to setValue.\n";
                exit(-1);
        }
    }
    for (int i = 0; i < fanout[gate]; i++) {
        successor = fnlist[gate][i];
        if (sched[successor] == 0) {
            insertEvent(levelNum[successor], successor);
            sched[successor] = 1;
        }
    }
    return 1;
}


////////////////////////////////////////////////////////////////////////
// applyVector()
//  This function applies the vector to the inputs of the ckt.
////////////////////////////////////////////////////////////////////////

void gateLevelCkt::applyVector(char *vec) {
    unsigned int origVal1, origVal2;
    char origBit;
    int successor;
    xlevelMAX = 0;

    /* reset the on and offlist */
    if (XTREE) {
        memset(xOnList, 0, sizeof(*xOnList) * (numgates+64));
        memset(xOffList, 0, sizeof(*xOffList) * (numgates+64));
    }

    for (int i = 0; i < numpri; i++) {
        origVal1 = value1[inputs[i]] & 1;
        origVal2 = value2[inputs[i]] & 1;
        if ((origVal1 == 1) && (origVal2 == 1)) {
            origBit = '1';
        } else if ((origVal1 == 0) && (origVal2 == 0)) {
            origBit = '0';
        } else {
            origBit = 'x';
        }

        if ((origBit != vec[i]) || (origBit == 'x') || (vec[i] == 'X')) {
            switch (vec[i]) {
            case '0':
                value1[inputs[i]] = 0;
                value2[inputs[i]] = 0;
                xlevel[inputs[i]] = 0;
                break;
            case '1':
                value1[inputs[i]] = ALLONES;
                value2[inputs[i]] = ALLONES;
                xlevel[inputs[i]] = 0;
                break;
            case 'x':
            case 'X':
                value1[inputs[i]] = 0;
                value2[inputs[i]] = ALLONES;
                xlevel[inputs[i]] = ++xlevelMAX;
                xtree[inputs[i]][xlevelMAX] |= X_TREE_ON;
                if (XTREE) {
                    xOnList[xlevelMAX] = inputs[i];
                }
                break;
            default:
                cerr << vec[i] << ": error in the input vector.\n";
                exit(-1);
            } // switch

            // different from previous time frame, place in wheel
            for (int j = 0; j < fanout[inputs[i]]; j++) {
                successor = fnlist[inputs[i]][j];
                if (sched[successor] == 0) {
                    insertEvent(levelNum[successor], successor);
                    sched[successor] = 1;
                }
            }
        }   // if ((origBit...)
    }   // for (i...)
}

////////////////////////////////////////////////////////////////////////
// lowWheel class
////////////////////////////////////////////////////////////////////////

void gateLevelCkt::setupWheel(int numLevels, int levelSize) {
    numlevels = numLevels;
    levelLen = new int[numLevels];
    levelEvents = new int * [numLevels];
    for (int i = 0; i < numLevels; i++) {
        levelEvents[i] = new int[levelSize];
        levelLen[i] = 0;
    }
    activation = new int[levelSize];
    
    actFFList = new int[numff + 1];
}

////////////////////////////////////////////////////////////////////////
int gateLevelCkt::retrieveEvent() {
    while ((levelLen[currLevel] == 0) && (currLevel < maxlevels)) {
        currLevel++;
    }

    if (currLevel < maxlevels) {
        levelLen[currLevel]--;
        return(levelEvents[currLevel][levelLen[currLevel]]);
    } else {
        return(-1);
    }
}

////////////////////////////////////////////////////////////////////////
// goodsim() -
//  Logic simulate. (no faults inserted)
////////////////////////////////////////////////////////////////////////

void gateLevelCkt::goodsim() {
    int sucLevel;
    int gateN, predecessor, successor;
    int *predList;
    unsigned int temp_xlevel;
    unsigned int numXs;
    unsigned int val1, val2, tmpVal;

    currLevel = 0;
    actLen = actFFLen = 0;
    while (currLevel < maxlevels) {
        gateN = retrieveEvent();
        if (gateN != -1) { // if a valid event
            sched[gateN]= 0;
            temp_xlevel = 0;
            numXs = 0;
            bool invert = false;
            bool newX = false;
            switch (gtype[gateN]) {
            case T_nand:
                invert = true;
            case T_and:
                val1 = val2 = ALLONES;
                predList = inlist[gateN];
                for (int i = 0; i < fanin[gateN]; i++) {
                    predecessor = predList[i];
                    if ((value1[predecessor] != value2[predecessor])) { /* Add predecessor's chain to the chain */
                        for (int j = 0; j <= xlevel[predecessor]; j++) {
                            xtree[gateN][j] |= xtree[predecessor][j];
                        }
                    }
                    if (value1[predecessor] == 0 && value2[predecessor] == 0) { /* controlling values are not considered squashed */
                        val1 = val2 = 0;
                        numXs = 0;
                        newX = false;
                        break;
                    } else if (newX) { /* check if a new X canceled with an existing X */
                        for (int j = 0; j <= i; j++) {
                            if (xtree[gateN][xlevel[predList[j]]] == (X_TREE_ON & X_TREE_OFF)) {
                                val1 = val2 = 0;
                                numXs = 2;
                                newX = false;
                                xlevelMAX--;
                                break;
                            }
                        }
                        /* something canceled and the output is now forced */
                        if (!newX) {
                            break;
                        }
                    } else if (temp_xlevel && xlevel[predecessor] && (temp_xlevel != xlevel[predecessor])) {
                        temp_xlevel = ++xlevelMAX;
                        val1 = 0;
                        val2 = ALLONES;
                        newX = true;
                    } else {
                        val1 &= value1[predecessor];
                        val2 &= value2[predecessor];
                        numXs += xlevel[predecessor] ? 1 : 0;
                        temp_xlevel = temp_xlevel ? temp_xlevel : xlevel[predecessor];
                        if (val1 == 0 && val2 == 0) { /* value has been squashed */
                            break;
                        }
                    }
                }
                break;
            case T_nor:
                invert = true;
            case T_or:
                val1 = val2 = 0;
                predList = inlist[gateN];
                for (int i = 0; i < fanin[gateN]; i++) {
                    predecessor = predList[i];
                    if ((value1[predecessor] != value2[predecessor])) { /* Add predecessor's chain to the chain */
                        for (int j = 0; j <= xlevel[predecessor]; j++) {
                            xtree[gateN][j] |= xtree[predecessor][j];
                        }
                    }
                    if (value1[predecessor] == ALLONES && value2[predecessor] == ALLONES) { /* controlling values are not considered squashed */
                        val1 = val2 = ALLONES;
                        numXs = 0;
                        newX = false;
                        break;
                    } else if (newX) { /* check if a new X canceled with an existing X */
                        for (int j = 0; j <= i; j++) {
                            if (xtree[gateN][xlevel[predList[j]]] == (X_TREE_ON & X_TREE_OFF)) {
                                val1 = val2 = ALLONES;
                                numXs = 2;
                                newX = false;
                                xlevelMAX--;
                                break;
                            }
                        }
                        /* something canceled and the output is now forced */
                        if (!newX) {
                            break;
                        }
                    } else if (temp_xlevel && xlevel[predecessor] && (temp_xlevel != xlevel[predecessor])) {
                        temp_xlevel = ++xlevelMAX;
                        val1 = 0;
                        val2 = ALLONES;
                        newX = true;
                    } else {
                        val1 |= value1[predecessor];
                        val2 |= value2[predecessor];
                        numXs += xlevel[predecessor] ? 1 : 0;
                        temp_xlevel = temp_xlevel ? temp_xlevel : xlevel[predecessor];
                        if (val1 == ALLONES && val2 == ALLONES) { /* value has been squashed */
                            break;
                        }
                    }
                }
                break;
            case T_not:
                invert = true;
            case T_buf:
            case T_dff:
            case T_output:
                predecessor = inlist[gateN][0];
                val1 = value1[predecessor];
                val2 = value2[predecessor];
                temp_xlevel = xlevel[predecessor];
                /* Add predecessor's chain to the chain */
                if (val1 != val2) { 
                    for (int j = 0; j <= xlevel[predecessor]; j++) {
                        xtree[gateN][j] |= xtree[predecessor][j];
                    }
                }
                if (gtype[gateN] == T_dff) {
                    actFFList[actFFLen] = gateN;
                    actFFLen++;
                }
                break;
            case T_xnor:
                invert = true;
            case T_xor:
                predList = inlist[gateN];
                val1 = 0;
                val2 = 0;

                for (int i = 0; i < fanin[gateN]; i++) {
                    predecessor = predList[i];
                    /* Add predecessor's chain to the chain */
                    if ((value1[predecessor] != value2[predecessor])) {
                        for (int j = 0; j < xlevel[predecessor]; j++) {
                            xtree[gateN][j] |= xtree[predecessor][j];
                        }
                        xtree[gateN][xlevel[predecessor]] ^= xtree[predecessor][xlevel[predecessor]];
                    }
                    if (newX) { /* check if a new X canceled with an existing X */
                        int check = 0, parity = 0;
                        bool multiX = false;
                        for (int j = 0; j <= i; j++) {
                            if (xtree[gateN][xlevel[predList[j]]]) {
                                /* if this isn't the first x predecessor existing */
                                if (check) {
                                    multiX = true;
                                } else { /* if this is the first, set check to it */
                                    check = predList[j];
                                }
                            } else if (value1[predList[j]] == ALLONES && value2[predList[j]] == ALLONES) {
                                parity++;
                            }
                        }
                        /* only one X was found, other Xs were eliminated by a new predecessor */
                        /* restore that value now */
                        if (!multiX) {
                            newX = false;
                            int inverse = xtree[gateN][check];
                            temp_xlevel = xlevel[check];
                            xtree[gateN][xlevelMAX] = X_TREE_NA;
                            numXs = 1;

                            val1 = (inverse & X_TREE_ON) ? ALLONES : 0;
                            val2 = (inverse & X_TREE_OFF) ? ALLONES : 0;
                            /* incorporate parity */

                            val1 = (parity % 2) ? val1 ^ ALLONES : val1;
                            val2 = (parity % 2) ? val2 ^ ALLONES : val2;
                            xlevelMAX--;              
                        }
                    } else if (temp_xlevel && xlevel[predecessor] && (temp_xlevel != xlevel[predecessor])) {
                        temp_xlevel = ++xlevelMAX;
                        val1 = 0;
                        val2 = ALLONES;
                        newX = true;
                    } else {
                        val1 ^= value1[predecessor];
                        val2 ^= value2[predecessor];
                        numXs += xlevel[predecessor] ? 1 : 0;
                        temp_xlevel = temp_xlevel ? temp_xlevel : xlevel[predecessor];
                    }
                }
                break;
            case T_input:
            case T_tie0:
            case T_tie1:
            case T_tieX:
                temp_xlevel = ++xlevelMAX;
                val1 = 0;
                val2 = ALLONES;
                newX = true;
                break;
            case T_tieZ:
                val1 = value1[gateN];
                val2 = value2[gateN];
                break;
            default:
                cerr << "illegal gate type1 " << gateN << " " << gtype[gateN] << "\n";
                exit(-1);
            }   // switch

            // set X level
            if (val1 == val2) {
                xlevel[gateN] = 0;
                if (numXs > 1) {
                    cout << "X's on inputs of gate " << gateN << " squashed!\n";
                }
            } else {
                xlevel[gateN] = temp_xlevel;
            }

            /* Apply inversion if applicable */
            if (invert && !newX) {
                val1 = ~val1;
                val2 = ~val2;
                if (val1 != val2) {
                    /* set new X value */
                    short temp = xtree[gateN][temp_xlevel];
                    xtree[gateN][temp_xlevel] |= (val1 == 0) ? X_TREE_ON : X_TREE_OFF;
                    if (XTREE && !(temp & X_TREE_OFF)) {
                        xOffList[temp_xlevel] = gateN;
                    }
                    if (XTREE && !(temp & X_TREE_ON)) {
                        xOnList[temp_xlevel] = gateN;
                    }
                }
            }
            /* add new X value */
            if (newX) { 
                xtree[gateN][temp_xlevel] |= X_TREE_ON;
                if (XTREE) {
                    xOnList[temp_xlevel] = gateN;
                }
            }

            // if gate value changed or value is x
            if ((val1 != value1[gateN]) || (val2 != value2[gateN]) || (val1 != val2)) {
                value1[gateN] = val1;
                value2[gateN] = val2;
                for (int i = 0; i < fanout[gateN]; i++) {
                    successor = fnlist[gateN][i];
                    sucLevel = levelNum[successor];
                    if (sched[successor] == 0) {
                        if (sucLevel != 0) {
                            insertEvent(sucLevel, successor);
                        } else { // same level, wrap around for next time
                            activation[actLen] = successor;
                            actLen++;
                        }
                    }
                    sched[successor] = 1;
                }   // for (i...)
            }   // if (val1..)
        }   // if (gateN...)
    }   // while (currLevel...)
    // now re-insert the activation list for the FF's
    for (int i = 0; i < actLen; i++) {
        insertEvent(0, activation[i]);
        sched[activation[i]] = 0;
        predecessor = inlist[activation[i]][0];
        gateN = ffMap[activation[i]];
        if (value1[predecessor]) {
            goodState[gateN] = '1';
        } else if (value2[predecessor] == 0) {
            goodState[gateN] = '0';
        } else {
            goodState[gateN] = 'X';
        }
    }
}

////////////////////////////////////////////////////////////////////////
// observeOutputs()
//  This function prints the outputs of the fault-free circuit.
////////////////////////////////////////////////////////////////////////

void gateLevelCkt::observeOutputs() {
    cout << "\t";
    for (int i = 0; i < numout; i++) {
        if (value1[outputs[i]] && value2[outputs[i]]) {
            cout << "1";
        } else if ((value1[outputs[i]] == 0) && (value2[outputs[i]] == 0)) {
            cout << "0";
        } else {
            cout << "X";
        }
    }

    cout << "\n";
    for (int i = 0; i < numff; i++) {
        if (value1[ff_list[i]] && value2[ff_list[i]]) {
            cout << "1";
        }
        else if ((value1[ff_list[i]] == 0) && (value2[ff_list[i]] == 0)) {
            cout << "0";
        }
        else {
            cout << "X";
        }
    }

    cout << "\n";
}

////////////////////////////////////////////////////////////////////////
// observeXTrees()
//  This function prints the X trees of the outputs of the ckt.
////////////////////////////////////////////////////////////////////////

void gateLevelCkt::observeXTrees() {
    for (int i = 0; i < numout; i++) {
        if (value1[outputs[i]] != value2[outputs[i]]) {
            cout << "Gate " << outputs[i] << "'s X-Tree:\n";
            printXTree(outputs[i]);
        }
    }
    cout << "\n";
}


////////////////////////////////////////////////////////////////////////
// printXTrees(int gate)
//  This function prints the X trees of the provided gate
////////////////////////////////////////////////////////////////////////
void gateLevelCkt::printXTree(int gate) {
    if (gate > numgates || gate < 1) {
        cout << "Invalid gate\n";
    } else {
        for (int i = 0; i < numgates; i++) {
            if (xtree[gate][i] & X_TREE_ON) {
                cout << "\tX" << i << "\tat gate " << xOnList[i] << endl;
            }
            if (xtree[gate][i] & X_TREE_OFF) {
                cout << "\tX" << i << "!\tat gate " << xOffList[i] << endl;
            }
        }
    }
}
