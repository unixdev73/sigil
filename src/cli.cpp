#include "sigil.hpp"
#include <cfgtk/lexer.hpp>
#include <cfgtk/parser.hpp>
#include <logger.hpp>
#include <string>
#include <vector>

namespace {
cfg::lexer_table_t make_lexer_table();
void print_summary(context *c);

bool validate(const std::vector<std::string> *, const cfg::lexer_table_t &,
              const cfg::grammar_t &, const cfg::action_map_t &,
              std::unordered_map<std::string, std::size_t> &);

void configure_singles(cfg::grammar_t *g);

void add_compress_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                       const cfg::action_t &count);
void add_verbose_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                      const cfg::action_t &count);
void add_debug_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                    const cfg::action_t &count);
void add_file_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                   const cfg::action_t &count);
void add_width_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                    const cfg::action_t &count);
void add_height_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                     const cfg::action_t &count);
void add_party_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                    const cfg::action_t &count);
void add_red_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                  const cfg::action_t &count);
void add_green_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                    const cfg::action_t &count);
void add_blue_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                   const cfg::action_t &count);
} // namespace

bool parse_cli(context *c, int argc, char **argv) {
  using cfg::add_rule;
  using cfg::bind;

  if (argc < 2) {
    logger l{logger::err};
    l.loge("Too few arguments\n");
    return false;
  }

  std::unordered_map<std::string, std::size_t> occmap{};
  const auto input = flt::to_container<std::vector>(argc, argv, 1);
  const auto tbl = make_lexer_table();
  cfg::action_map_t m{};
  cfg::grammar_t g{};
  logger l{};

  const cfg::action_t count = [&occmap](auto *, auto *l, auto *) {
    if (!l)
      return;
    if (!occmap.contains(l->value))
      occmap.emplace(l->value, 1);
    else
      ++occmap.at(l->value);
  };

  auto r = add_rule(&g, "start", "help-flag");
  bind(&m, r, count);
  bind(&m, r, [c](auto *, auto *, auto *) { c->help = true; });

  add_compress_rule(c, g, m, count);
  add_verbose_rule(c, g, m, count);
  add_debug_rule(c, g, m, count);
  add_file_rule(c, g, m, count);
  add_width_rule(c, g, m, count);
  add_height_rule(c, g, m, count);
  add_party_rule(c, g, m, count);
  add_red_rule(c, g, m, count);
  add_green_rule(c, g, m, count);
  add_blue_rule(c, g, m, count);

  add_rule(&g, "start", "arg", "arg_list");
  add_rule(&g, "arg_list", "arg", "arg_list");
  add_rule(&g, "width-option#0", "width-option");
  add_rule(&g, "size-tok#0", "size-tok");
  add_rule(&g, "string-tok#0", "string-tok");
  add_rule(&g, "string-tok#0", "size-tok");
  add_rule(&g, "height-option#0", "height-option");
  add_rule(&g, "file-option#0", "file-option");
  add_rule(&g, "party-option#0", "party-option");
  add_rule(&g, "red#0", "red");
  add_rule(&g, "green#0", "green");
  add_rule(&g, "blue#0", "blue");

  if (!validate(&input, tbl, g, m, occmap))
    return false;

  print_summary(c);
  return true;
}

namespace {
void add_blue_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                   const cfg::action_t &count) {
  std::string col{"blue#0"};
  {
    auto r = add_rule(&g, "start", col, "string-tok#0");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *r) {
      c->blue = float(std::stoull(r->value)) / 255.f;
    });
  }
  {
    auto r = add_rule(&g, "arg_list", col, "string-tok#0");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *r) {
      c->blue = float(std::stoull(r->value)) / 255.f;
    });
  }
  {
    auto r = add_rule(&g, "arg", col, "string-tok#0");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *r) {
      c->blue = float(std::stoull(r->value)) / 255.f;
    });
  }
}

void add_green_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                    const cfg::action_t &count) {
  std::string col{"green#0"};
  {
    auto r = add_rule(&g, "start", col, "string-tok#0");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *r) {
      c->green = float(std::stoull(r->value)) / 255.f;
    });
  }
  {
    auto r = add_rule(&g, "arg_list", col, "string-tok#0");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *r) {
      c->green = float(std::stoull(r->value)) / 255.f;
    });
  }
  {
    auto r = add_rule(&g, "arg", col, "string-tok#0");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *r) {
      c->green = float(std::stoull(r->value)) / 255.f;
    });
  }
}

void add_red_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                  const cfg::action_t &count) {
  std::string col{"red#0"};
  {
    auto r = add_rule(&g, "start", col, "string-tok#0");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *r) {
      c->red = float(std::stoull(r->value)) / 255.f;
    });
  }
  {
    auto r = add_rule(&g, "arg_list", col, "string-tok#0");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *r) {
      c->red = float(std::stoull(r->value)) / 255.f;
    });
  }
  {
    auto r = add_rule(&g, "arg", col, "string-tok#0");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *r) {
      c->red = float(std::stoull(r->value)) / 255.f;
    });
  }
}

void add_party_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                    const cfg::action_t &count) {
  {
    auto r = add_rule(&g, "start", "party-option#0", "string-tok#0");
    bind(&m, r, count);
    bind(&m, r,
         [c](auto *, auto *, auto *r) { c->party = std::stoull(r->value); });
  }
  {
    auto r = add_rule(&g, "arg_list", "party-option#0", "string-tok#0");
    bind(&m, r, count);
    bind(&m, r,
         [c](auto *, auto *, auto *r) { c->party = std::stoull(r->value); });
  }
  {
    auto r = add_rule(&g, "arg", "party-option#0", "string-tok#0");
    bind(&m, r, count);
    bind(&m, r,
         [c](auto *, auto *, auto *r) { c->party = std::stoull(r->value); });
  }
}

void add_height_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                     const cfg::action_t &count) {
  {
    auto r = add_rule(&g, "start", "height-option#0", "size-tok#0");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *s) {
      c->window_height = std::stoull(s->value);
    });
  }
  {
    auto r = add_rule(&g, "arg_list", "height-option#0", "size-tok#0");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *s) {
      c->window_height = std::stoull(s->value);
    });
  }
  {
    auto r = add_rule(&g, "arg", "height-option#0", "size-tok#0");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *s) {
      c->window_height = std::stoull(s->value);
    });
  }
}

void add_width_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                    const cfg::action_t &count) {
  {
    auto r = add_rule(&g, "start", "width-option#0", "size-tok#0");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *s) {
      c->window_width = std::stoull(s->value);
    });
  }
  {
    auto r = add_rule(&g, "arg_list", "width-option#0", "size-tok#0");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *s) {
      c->window_width = std::stoull(s->value);
    });
  }
  {
    auto r = add_rule(&g, "arg", "width-option#0", "size-tok#0");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *s) {
      c->window_width = std::stoull(s->value);
    });
  }
}

void add_file_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                   const cfg::action_t &count) {
  {
    auto r = add_rule(&g, "start", "file-option#0", "string-tok#0");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *s) { c->matrix_file = s->value; });
  }
  {
    auto r = add_rule(&g, "arg_list", "file-option#0", "string-tok#0");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *s) { c->matrix_file = s->value; });
  }
  {
    auto r = add_rule(&g, "arg", "file-option#0", "string-tok#0");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *s) { c->matrix_file = s->value; });
  }
}

void add_debug_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                    const cfg::action_t &count) {
  {
    auto r = add_rule(&g, "start", "debug-flag");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *) { c->debug = true; });
  }
  {
    auto r = add_rule(&g, "arg_list", "debug-flag");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *) { c->debug = true; });
  }
  {
    auto r = add_rule(&g, "arg", "debug-flag");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *) { c->debug = true; });
  }
}

void add_verbose_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                      const cfg::action_t &count) {
  const auto lvl = logger::inf | logger::wrn | logger::err;

  {
    auto r = add_rule(&g, "start", "verbose-flag");
    bind(&m, r, count);
    bind(&m, r, [c, lvl](auto *, auto *, auto *) { c->log_level = lvl; });
  }
  {
    auto r = add_rule(&g, "arg_list", "verbose-flag");
    bind(&m, r, count);
    bind(&m, r, [c, lvl](auto *, auto *, auto *) { c->log_level = lvl; });
  }
  {
    auto r = add_rule(&g, "arg", "verbose-flag");
    bind(&m, r, count);
    bind(&m, r, [c, lvl](auto *, auto *, auto *) { c->log_level = lvl; });
  }
}

void add_compress_rule(context *c, cfg::grammar_t &g, cfg::action_map_t &m,
                       const cfg::action_t &count) {
  {
    auto r = add_rule(&g, "start", "compress-flag");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *) { c->compress = true; });
  }
  {
    auto r = add_rule(&g, "arg_list", "compress-flag");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *) { c->compress = true; });
  }
  {
    auto r = add_rule(&g, "arg", "compress-flag");
    bind(&m, r, count);
    bind(&m, r, [c](auto *, auto *, auto *) { c->compress = true; });
  }
}

bool validate(const std::vector<std::string> *input,
              const cfg::lexer_table_t &tbl, const cfg::grammar_t &g,
              const cfg::action_map_t &m,
              std::unordered_map<std::string, std::size_t> &occmap) {

  logger l{logger::err};
  const auto tokens = cfg::tokenize(&tbl, input);
  const auto chart = cfg::cyk(&g, &tokens, &m);

  if (!cfg::is_valid(&chart, cfg::get_start(&g))) {
    l.loge("Parsing failed\n");
    std::cerr << cfg::to_string(&chart) << "\n";
    return false;
  }

  const auto trees = cfg::get_trees(&chart, cfg::get_start(&g));
  try {
    cfg::run_actions(&trees.front(), &m);
  } catch (...) {
    l.loge("Caught an exception during semantic action execution\n");
    return false;
  }

  for (const auto &e : occmap)
    if (e.second > 1) {
      l.loge("The occurrence of '", e.first, "' is greater than 1: ", e.second,
             "\n");
      return false;
    }

  return true;
}

cfg::lexer_table_t make_lexer_table() {
  cfg::lexer_table_t tbl{};
  cfg::add_entry(&tbl, cfg::token_type::flag, "compress-flag", "-c|--compress");
  cfg::add_entry(&tbl, cfg::token_type::flag, "verbose-flag", "-v|--verbose");
  cfg::add_entry(&tbl, cfg::token_type::flag, "help-flag", "--help");
  cfg::add_entry(&tbl, cfg::token_type::flag, "debug-flag", "-d|--debug");
  cfg::add_entry(&tbl, cfg::token_type::option, "party-option", "-p|--party");
  cfg::add_entry(&tbl, cfg::token_type::option, "file-option", "-f|--file");
  cfg::add_entry(&tbl, cfg::token_type::option, "width-option", "-w|--width");
  cfg::add_entry(&tbl, cfg::token_type::option, "height-option", "-h|--height");
  cfg::add_entry(&tbl, cfg::token_type::option, "red", "-r|--red");
  cfg::add_entry(&tbl, cfg::token_type::option, "green", "-g|--green");
  cfg::add_entry(&tbl, cfg::token_type::option, "blue", "-b|--blue");
  cfg::add_entry(&tbl, cfg::token_type::free, "size-tok", "[1-9]\\d{2,3}");
  cfg::add_entry(&tbl, cfg::token_type::free, "string-tok", ".+");
  return tbl;
}

void print_summary(context *c) {
  logger l{c->log_level};
  l.logi("Parsing complete; the following variables have been set:\n");
  l.logs("\tverbose: true\n");
  l.logs("\tcompress: ", c->compress ? "true" : "false", "\n");
  l.logs("\tparty: ", c->party ? "true" : "false", "\n");
  l.logs("\tdebug: ", c->debug ? "true" : "false", "\n");
  l.logs("\twindow width: ", c->window_width, "\n");
  l.logs("\twindow height: ", c->window_height, "\n");
  l.logs("\tmatrix file: ", c->matrix_file, "\n");
  l.logs("\tsigil color: ", c->red, ", ", c->green, ", ", c->blue, "\n");
}
} // namespace
