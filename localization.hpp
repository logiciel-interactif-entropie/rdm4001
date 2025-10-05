#pragma once
#include <string>
#include <unordered_map>
#include <vector>
namespace rdm {
class LocalizationManager {
  LocalizationManager();

  bool dirty;
  std::vector<std::string> languageBases;
  std::string language;
  std::unordered_map<int, std::string> localizedStrings;

  void loadStrings();

 public:
  static LocalizationManager* singleton();
  static constexpr int hash(const char* input) {
    return *input ? static_cast<unsigned int>(*input) + 33 * hash(input + 1)
                  : 5381;
  }

  void addLocalizationBase(std::string base);
  void setLanguage(std::string lang);

  const char* get(int token, const char* subst) const;
};
};  // namespace rdm

#define Lc(token, subst)                      \
  rdm::LocalizationManager::singleton()->get( \
      rdm::LocalizationManager::hash(#token), subst)
