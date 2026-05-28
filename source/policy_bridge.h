#pragma once
#ifndef POLICY_BRIDGE_H
#define POLICY_BRIDGE_H

#include "threads.h"

void init_policy();
bool load_policy(const char* filename);
void evaluate_policy(ThreadData& td, float* output_scores);

#endif