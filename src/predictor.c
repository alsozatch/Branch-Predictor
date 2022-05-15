//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <math.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "Zachary Adler";
const char *studentID   = "A15051602";
const char *email       = "zsadler@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

//define number of bits required for indexing the BHT here. 
int ghistoryBits = 14; // Number of bits used for Global History
int bpType;       // Branch Prediction Type
int verbose;


// tournament

//int lhtBits = 10; // number of bits of pc used to index the local history
//int lbhtBits = 10; // number of local history bits used to get prediction
//int gbhtBits = 12;
//int cbhtBits = 12;

//int lht_entries = 1 << lhtBits; // 2 ^{10}
//int lbht_entries = 1 << lbhtBits; // 2 ^{10}
//int gbht_entries = 1 << gbhtBits; // 2 ^{12}
//int cbht_entries = 1 << cbhtBits; // 2 ^{12}

//int lht_entries = 1 << 10; // 2 ^{10}
//int lbht_entries = 1 << 10; // 2 ^{10}
//int gbht_entries = 1 << 12; // 2 ^{12}
//int cbht_entries = 1 << 12; // 2 ^{12}

int lht_entries = 1 << 10;
int lbht_entries = 1 << 13;
int gbht_entries = 1 << 13;
int cbht_entries = 1 << 13;


// perceptron
int pc_entries = 1 << 7;
uint16_t weight_entries = 32; // 1 bit reserved for bias, so it is 1 more than number of global history bits used

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
//TODO: Add your own Branch Predictor data structures here
//
//gshare
uint8_t *bht_gshare;
uint64_t ghistory;

//tournament
uint16_t *lht; // local history table
uint8_t *lbht; // local prediction BHT
uint8_t *gbht; // global prediction BHT
uint8_t *cbht; // choice prediction BHT
uint64_t ghistory_tournament; // global history

// perceptron
int8_t ptable[1 << 7][32]; // 1 bias + the rest ghistory bits for columns
uint64_t ghistory_perceptron;
int theta;

// perceptron tournament
int gbht_pt_entries = 1 << 13;
int cbht_pt_entries = 1 << 11;


//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// perceptron vs gshare tournament functions
void init_pt() {
    
    // PERCEPTRON
    int i, j;
    for (i = 0; i < pc_entries; i++) {
        for (j = 0; j < weight_entries; j++) {
            ptable[i][j] = -1;
        }
    }
    
    theta = (int)(1.93 * (weight_entries - 1) + 14);
    
    // GSHARE
    gbht = (uint8_t*)malloc(gbht_pt_entries * sizeof(uint8_t));
    for (i = 0; i < gbht_pt_entries; i++){
        gbht[i] = WN;
    }
    
    
    // CHOOSER
    cbht = (uint8_t*)malloc(cbht_pt_entries * sizeof(uint8_t));
    for (i = 0; i < cbht_pt_entries; i++){
        cbht[i] = WN;
    }
    
    
    ghistory = 0;
}

int pt_calculate_y(uint32_t pc) {
    uint32_t pc_lower_bits = pc & (pc_entries - 1);
    
    int y = 0;
    y += ptable[pc_lower_bits][0];
    
    int i;
    for (i = 1; i < weight_entries; i++) {
        if ((ghistory & (1 << (i-1))) == 0) {
            y -= ptable[pc_lower_bits][i];
        }
        else {
            y += ptable[pc_lower_bits][i];
        }
    }
    
    return y;
}

uint8_t pt_predict_perceptron(uint32_t pc) {
    int y = pt_calculate_y(pc);
    return (y >= 0) ? TAKEN : NOTTAKEN;
}

uint8_t pt_predict_gshare(uint32_t pc) {
    uint32_t ghistory_lower_bits = ghistory & (gbht_pt_entries - 1);
    uint32_t pc_lower_bits = pc & (gbht_pt_entries - 1);
    uint32_t index = ghistory_lower_bits ^ pc_lower_bits;
    switch (gbht[index]) {
        case WN:
            return NOTTAKEN;
        case SN:
            return NOTTAKEN;
        case WT:
            return TAKEN;
        case ST:
            return TAKEN;
        default:
            printf("Warning: Undefined state of entry in GBHT!\n");
            return NOTTAKEN;
    }
}

uint8_t pt_predict(uint32_t pc) {
    uint32_t choice_lower_bits = ghistory & (cbht_pt_entries - 1);
    switch (cbht[choice_lower_bits]) {
        case WN:
            return pt_predict_gshare(pc);
        case SN:
            return pt_predict_gshare(pc);
        case WT:
            return pt_predict_perceptron(pc);
        case ST:
            return pt_predict_perceptron(pc);
        default:
            printf("Warning: Undefined state of entry in CHOICE BHT!\n");
            return pt_predict_gshare(pc);
    }
}

void train_pt(uint32_t pc, uint8_t outcome) {
    uint32_t pc_lower_bits = pc & (pc_entries - 1);
    uint32_t ghistory_lower_bits = ghistory & (gbht_pt_entries - 1);
    uint32_t choice_lower_bits = ghistory & (cbht_pt_entries - 1);
    uint32_t gshare_index = ghistory_lower_bits ^ (pc & (gbht_pt_entries - 1));
    
    // get perceptron prediction before updating it
    int y = pt_calculate_y(pc);
    uint8_t perceptron_prediction = (y >= 0) ? TAKEN : NOTTAKEN;;
    
    // get gshare prediction before updating it
    uint8_t gshare_prediction = pt_predict_gshare(pc);
    
    // update choice
    int8_t update_rule = (int8_t)((int16_t)perceptron_prediction - (int16_t)gshare_prediction); // 1, 0, or -1
    if (update_rule != 0) {
        
        switch (cbht[choice_lower_bits]) {
            case WN:
                cbht[choice_lower_bits] = (update_rule == 1) ? WT : SN;
                break;
            case SN:
                cbht[choice_lower_bits] = (update_rule == 1) ? WN : SN;
                break;
            case WT:
                cbht[choice_lower_bits] = (update_rule == 1) ? ST : WN;
                break;
            case ST:
                cbht[choice_lower_bits] = (update_rule == 1) ? ST : WT;
                break;
            default:
                printf("Warning: Undefined state of entry in CBHT");
        }

    }
    
    // train perceptron
    int8_t t = (outcome == TAKEN) ? 1 : -1;
    int8_t sign_y = (y >= 0) ? 1 : -1;
    
    int i;
    if ((sign_y != t) || (abs(y) <= theta)) {
        ptable[pc_lower_bits][0] += t;
        for (i = 1; i < weight_entries; i++) {
            if ((ghistory & (1 << (i-1))) == 0) {
                ptable[pc_lower_bits][i] -= t;
            }
            else {
                ptable[pc_lower_bits][i] += t;
            }
        }
    }
    
    // train gshare
    switch (gbht[gshare_index]) {
        case WN:
            gbht[gshare_index] = (outcome == TAKEN) ? WT : SN;
            break;
        case SN:
            gbht[gshare_index] = (outcome == TAKEN) ? WN : SN;
            break;
        case WT:
            gbht[gshare_index] = (outcome == TAKEN) ? ST : WN;
            break;
        case ST:
            gbht[gshare_index] = (outcome == TAKEN) ? ST : WT;
            break;
        default:
            printf("Warning: Undefined state of entry in GBHT");
    }
    
    ghistory = ((ghistory << 1) | outcome);
}



// perceptron functions
void init_perceptron() {
    int i, j;
    for (i = 0; i < pc_entries; i++) {
        for (j = 0; j < weight_entries; j++) {
            ptable[i][j] = 1;
        }
    }
    
    theta = (int)(1.93 * (weight_entries - 1) + 14);
    ghistory = 0;
}




int perceptron_calculate_y(uint32_t pc) {
    uint32_t pc_lower_bits = pc & (pc_entries - 1);
    
    int y = 0;
    y += ptable[pc_lower_bits][0];
    
    int i;
    for (i = 1; i < weight_entries; i++) {
        if ((ghistory_perceptron & (1 << (i-1))) == 0) {
            y -= ptable[pc_lower_bits][i];
        }
        else {
            y += ptable[pc_lower_bits][i];
        }
    }
    
    return y;
}

uint8_t perceptron_predict(uint32_t pc) {
    int y = perceptron_calculate_y(pc);
    return (y >= 0) ? TAKEN : NOTTAKEN;
}

void train_perceptron(uint32_t pc, uint8_t outcome) {
    uint32_t pc_lower_bits = pc & (pc_entries - 1);
    int y = perceptron_calculate_y(pc);
    int8_t t = (outcome == TAKEN) ? 1 : -1;
    int8_t sign_y = (y >= 0) ? 1 : -1;
    
    int i;
    if ((sign_y != t) || (abs(y) <= theta)) {
        ptable[pc_lower_bits][0] += t;
        for (i = 1; i < weight_entries; i++) {
            if ((ghistory_perceptron & (1 << (i-1))) == 0) {
                ptable[pc_lower_bits][i] -= t;
            }
            else {
                ptable[pc_lower_bits][i] += t;
            }
        }
    }
    
    ghistory_perceptron = ((ghistory_perceptron << 1) | outcome);
}

//tournament functions
void init_tournament() {
    int i;
    
    lht = (uint16_t*)malloc(lht_entries * sizeof(uint16_t));
    for (i = 0; i < lht_entries; i++){
        lht[i] = 0;
    }
    
    lbht = (uint8_t*)malloc(lbht_entries * sizeof(uint8_t));
    for (i = 0; i < lbht_entries; i++){
        lbht[i] = WN;
    }
    
    
    gbht = (uint8_t*)malloc(gbht_entries * sizeof(uint8_t));
    for (i = 0; i < gbht_entries; i++){
        gbht[i] = WN;
    }
    
    
    cbht = (uint8_t*)malloc(cbht_entries * sizeof(uint8_t));
    for (i = 0; i < cbht_entries; i++){
        cbht[i] = WN;
    }
    
    ghistory_tournament = 0;
}

uint8_t tournament_predict_local(uint32_t pc) {
    uint32_t pc_lower_bits = pc & (lht_entries - 1);
    uint16_t lhistory = lht[pc_lower_bits];
    uint16_t lhistory_lower_bits = lhistory & (lbht_entries - 1);
    switch (lbht[lhistory_lower_bits]) {
        case WN:
          return NOTTAKEN;
        case SN:
          return NOTTAKEN;
        case WT:
          return TAKEN;
        case ST:
          return TAKEN;
        default:
          printf("Warning: Undefined state of entry in LBHT!\n");
          return NOTTAKEN;
    }
}

uint8_t tournament_predict_global() {
    uint32_t ghistory_lower_bits = ghistory_tournament & (gbht_entries - 1);
    switch (gbht[ghistory_lower_bits]) {
        case WN:
          return NOTTAKEN;
        case SN:
          return NOTTAKEN;
        case WT:
          return TAKEN;
        case ST:
          return TAKEN;
        default:
          printf("Warning: Undefined state of entry in GBHT!\n");
          return NOTTAKEN;
    }
}

uint8_t tournament_predict(uint32_t pc) {
    uint32_t choice_lower_bits = ghistory_tournament & (cbht_entries - 1);
    switch (cbht[choice_lower_bits]) {
        case WN:
          return tournament_predict_local(pc);
        case SN:
          return tournament_predict_local(pc);
        case WT:
          return tournament_predict_global();
        case ST:
          return tournament_predict_global();
        default:
          printf("Warning: Undefined state of entry in CHOICE BHT!\n");
          return tournament_predict_local(pc);
    }
}

void train_tournament(uint32_t pc, uint8_t outcome) {
    uint32_t pc_lower_bits = pc & (lht_entries - 1);
    uint16_t lhistory = lht[pc_lower_bits];
    uint16_t lhistory_lower_bits = lhistory & (lbht_entries - 1);
    uint32_t ghistory_lower_bits = ghistory_tournament & (gbht_entries - 1);
    uint32_t choice_lower_bits = ghistory_tournament & (cbht_entries - 1);

    // obtain local history prediction before updating it
    uint8_t local_prediction;
    switch (lbht[lhistory_lower_bits]) {
        case WN:
            local_prediction = NOTTAKEN;
            break;
        case SN:
            local_prediction = NOTTAKEN;
            break;
        case WT:
            local_prediction = TAKEN;
            break;
        case ST:
            local_prediction = TAKEN;
            break;
        default:
            printf("Warning: Undefined state of entry in LBHT!\n");
            local_prediction = NOTTAKEN;
    }
    
    
    // obtain global history prediction before updating it
    uint8_t global_prediction;
    switch (gbht[ghistory_lower_bits]) {
        case WN:
            global_prediction = NOTTAKEN;
            break;
        case SN:
            global_prediction = NOTTAKEN;
            break;
        case WT:
            global_prediction = TAKEN;
            break;
        case ST:
            global_prediction = TAKEN;
            break;
        default:
            printf("Warning: Undefined state of entry in GBHT!\n");
            global_prediction = NOTTAKEN;
    }
    
    
    // update local history prediction
    switch (lbht[lhistory_lower_bits]) {
        case WN:
            lbht[lhistory_lower_bits] = (outcome == TAKEN) ? WT : SN;
            break;
        case SN:
            lbht[lhistory_lower_bits] = (outcome == TAKEN) ? WN : SN;
            break;
        case WT:
            lbht[lhistory_lower_bits] = (outcome == TAKEN) ? ST : WN;
            break;
        case ST:
            lbht[lhistory_lower_bits] = (outcome == TAKEN) ? ST : WT;
            break;
        default:
            printf("Warning: Undefined state of entry in LBHT");
    }
    
    // update global history prediction
    switch (gbht[ghistory_lower_bits]) {
        case WN:
            gbht[ghistory_lower_bits] = (outcome == TAKEN) ? WT : SN;
            break;
        case SN:
            gbht[ghistory_lower_bits] = (outcome == TAKEN) ? WN : SN;
            break;
        case WT:
            gbht[ghistory_lower_bits] = (outcome == TAKEN) ? ST : WN;
            break;
        case ST:
            gbht[ghistory_lower_bits] = (outcome == TAKEN) ? ST : WT;
            break;
        default:
            printf("Warning: Undefined state of entry in GBHT");
    }
    
    // update choice
    int8_t update_rule = (int8_t)((int16_t)global_prediction - (int16_t)local_prediction); // 1, 0, or -1
    if (update_rule != 0) {
        
        switch (cbht[choice_lower_bits]) {
            case WN:
                cbht[choice_lower_bits] = (update_rule == 1) ? WT : SN;
                break;
            case SN:
                cbht[choice_lower_bits] = (update_rule == 1) ? WN : SN;
                break;
            case WT:
                cbht[choice_lower_bits] = (update_rule == 1) ? ST : WN;
                break;
            case ST:
                cbht[choice_lower_bits] = (update_rule == 1) ? ST : WT;
                break;
            default:
                printf("Warning: Undefined state of entry in CBHT");
        }
        
    }
    
    // update local and global history recordings
    lht[pc_lower_bits] = ((lht[pc_lower_bits] << 1) | outcome);
    ghistory_tournament = ((ghistory_tournament << 1) | outcome);
}

void cleanup_tournament() {
    free(lht);
    free(lbht);
    free(gbht);
    free(cbht);
}


//gshare functions
// initializes bht_gshare to an array of 2^{14} uint8_t, since 14 bits are used to index bht_ghsare, each initialized to WN
// initializes ghistory to all 0
void init_gshare() {
 int bht_entries = 1 << ghistoryBits; // 2 ^{14}
  bht_gshare = (uint8_t*)malloc(bht_entries * sizeof(uint8_t));
  int i = 0;
  for(i = 0; i< bht_entries; i++){
    bht_gshare[i] = WN;
  }
  ghistory = 0;
}

uint8_t 
gshare_predict(uint32_t pc) {
  //get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries-1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries -1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  switch(bht_gshare[index]){
    case WN:
      return NOTTAKEN;
    case SN:
      return NOTTAKEN;
    case WT:
      return TAKEN;
    case ST:
      return TAKEN;
    default:
      printf("Warning: Undefined state of entry in GSHARE BHT!\n");
      return NOTTAKEN;
  }
}

void
train_gshare(uint32_t pc, uint8_t outcome) {
  //get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries-1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries -1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;

  //Update state of entry in bht based on outcome
  switch(bht_gshare[index]){
    case WN:
      bht_gshare[index] = (outcome==TAKEN)?WT:SN;
      break;
    case SN:
      bht_gshare[index] = (outcome==TAKEN)?WN:SN;
      break;
    case WT:
      bht_gshare[index] = (outcome==TAKEN)?ST:WN;
      break;
    case ST:
      bht_gshare[index] = (outcome==TAKEN)?ST:WT;
      break;
    default:
      printf("Warning: Undefined state of entry in GSHARE BHT!\n");
  }

  //Update history register
  ghistory = ((ghistory << 1) | outcome); 
}

void
cleanup_gshare() {
  free(bht_gshare);
}



void init_predictor()
{
    switch (bpType) {
    case STATIC:
            break;
    case GSHARE:
            init_gshare();
            break;
    case TOURNAMENT:
            init_tournament();
            break;
    case CUSTOM:
            init_pt();
            break;
    default:
            break;
  }
  
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t make_prediction(uint32_t pc)
{

    // Make a prediction based on the bpType
    switch (bpType) {
        case STATIC:
            return TAKEN;
    case GSHARE:
            return gshare_predict(pc);
    case TOURNAMENT:
            return tournament_predict(pc);
    case CUSTOM:
            return pt_predict(pc);
    default:
            break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//

void train_predictor(uint32_t pc, uint8_t outcome)
{
    switch (bpType) {
    case STATIC:
    case GSHARE:
            return train_gshare(pc, outcome);
    case TOURNAMENT:
            return train_tournament(pc, outcome);
    case CUSTOM:
            return train_pt(pc, outcome);
    default:
            break;
  }
  

}
