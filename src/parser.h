#pragma once
#include "ir.h"
#include <optional>
#include <string>

std::optional<Graph> parseByteCode(const std::string &path, std::string *err = nullptr);
void buildBlocks(Graph &P);
