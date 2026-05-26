#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <stdio.h>
#include <ctype.h>

template<typename T>
class CommandParser {
  public:
    CommandParser(T *obj,
                  void (T::*info)(int),
                  void (T::*mode)(int),
                  void (T::*getter)(int),
                  void (T::*setter)(int, int),
                  void (T::*error)(const char *));
    void parseInput(char input);

  private:
    void doCommand();

    char command[256] = "";
    int position = 0;

    T *_obj;
    void (T::*infoFunction)(int);
    void (T::*modeFunction)(int);
    void (T::*getFunction)(int);
    void (T::*setFunction)(int, int);
    void (T::*errorFunction)(const char *);
};

template<typename T>
CommandParser<T>::CommandParser(T *obj,
                                void (T::*info)(int),
                                void (T::*mode)(int),
                                void (T::*getter)(int),
                                void (T::*setter)(int, int),
                                void (T::*error)(const char *))
  : _obj(obj), infoFunction(info), modeFunction(mode),
    getFunction(getter), setFunction(setter), errorFunction(error) {
}

template<typename T>
void CommandParser<T>::parseInput(char input) {
  if (input != '\n' && input != '\r') {
    command[position++] = input;
  } else {
    command[position] = 0;
    if (position != 0) {
      doCommand();
      position = 0;
    }
  }
}

template<typename T>
void CommandParser<T>::doCommand() {
  int index;
  int intParam;

  command[0] = toupper(command[0]);

  switch (command[0]) {
    case 'I':
      if (sscanf(command, "I%d", &index) > 0) {
        (_obj->*infoFunction)(index);
      } else {
        (_obj->*errorFunction)(command);
      }
      break;
    case 'M':
      if (sscanf(command, "M%d", &index) > 0) {
        (_obj->*modeFunction)(index);
      } else {
        (_obj->*errorFunction)(command);
      }
      break;
    case 'G':
      if (sscanf(command, "G%d", &index) > 0) {
        (_obj->*getFunction)(index);
      } else {
        (_obj->*errorFunction)(command);
      }
      break;
    case 'S':
      intParam = -1;
      if (sscanf(command, "S%d %d", &index, &intParam) > 0) {
        (_obj->*setFunction)(index, intParam);
      } else {
        (_obj->*errorFunction)(command);
      }
      break;
    default:
      (_obj->*errorFunction)(command);
      break;
  }
}

#endif // COMMAND_PARSER_H
