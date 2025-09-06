//*****************************************************************************
//
// pymud.h
//
// a python module that provides some useful utility functions for interacting
// with the MUD. Includes stuff like global variables, messaging functions,
// and a whole bunch of other stuff.
//
// WORK FOR FUTURE: Our use of Py_INCREF is probably creating a
//                  memory leak somewhere.
//
//*****************************************************************************

#include <Python.h>
#include <structmember.h>

#include "../mud.h"
#include "../utils.h"
#include "../character.h"
#include "../inform.h"
#include "../handler.h"
#include "../parse.h"
#include "../races.h"

#include "scripts.h"
#include "pyroom.h"
#include "pychar.h"
#include "pyobj.h"
#include "pyplugs.h"
#include "pyexit.h"
#include "pysocket.h"



//*****************************************************************************
// local variables and functions
//*****************************************************************************
// global variables we have set.
PyObject  *globals = NULL;

// a list of methods to add to the mud module
LIST *pymud_methods = NULL;



//*****************************************************************************
//
// GLOBAL VARIABLES
//
// the following functions allow scriptors to store/access global variables.
// globals are stored in a python map, that maps two python objects together.
// the functions used to interact with globals are:
//   get_global(key)
//   set_global(key, val)
//   erase_global(key)
//
//*****************************************************************************
PyObject *mud_get_global(PyObject *self, PyObject *args) {
  PyObject *key = NULL;

  // get the key
  if (!PyArg_ParseTuple(args, "O", &key)) {
    PyErr_Format(PyExc_TypeError, 
		 "Could not retrieve global variable - no key provided");
    return NULL;
  }

  PyObject *val = PyDict_GetItem(globals, key);
  if(val == NULL)
    val = Py_None;
  
  Py_INCREF(val);
  return val;
}

PyObject *mud_set_global(PyObject *self, PyObject *args) {
  PyObject *key = NULL, *val = NULL;

  if (!PyArg_ParseTuple(args, "OO", &key, &val)) {
    PyErr_Format(PyExc_TypeError, 
		 "Could not set global variable - need key and value");
    return NULL;
  }

  PyDict_SetItem(globals, key, val);
  return Py_BuildValue("i", 1);
}


PyObject *mud_erase_global(PyObject *self, PyObject *args) {
  PyObject *key = NULL;

  if (!PyArg_ParseTuple(args, "O", &key)) {
    PyErr_Format(PyExc_TypeError, 
		 "Could not erase global variable - need key");
    return NULL;
  }

  PyDict_SetItem(globals, key, Py_None);
  return Py_BuildValue("i", 1);
}


//
// format a string to be into a typical description style
PyObject *mud_format_string(PyObject *self, PyObject *args) {
  char *string = NULL;
  bool indent  = TRUE;
  int  width   = SCREEN_WIDTH;

  // parse all of the values
  if (!PyArg_ParseTuple(args, "s|bi", &string, &indent, &width)) {
    PyErr_Format(PyExc_TypeError, 
		 "Can not format non-string values.");
    return NULL;
  }

  // dup the string so we can work with it and not intrude on the PyString data
  BUFFER *buf = newBuffer(MAX_BUFFER);
  bufferCat(buf, string);
  bufferFormat(buf, width, (indent ? PARA_INDENT : 0));
  PyObject *ret = Py_BuildValue("s", bufferString(buf));
  deleteBuffer(buf);
  return ret;
}


//
// parses arguments for character commands
PyObject *mud_parse_args(PyObject *self, PyObject *args) {
  PyObject   *pych = NULL;
  bool show_errors = FALSE;
  char        *cmd = NULL;
  char     *pyargs = NULL;
  char     *syntax = NULL;
  char *parse_args = NULL;
  CHAR_DATA    *ch = NULL;

  // parse our arguments
  if(!PyArg_ParseTuple(args, "Obsss", &pych, &show_errors, 
		       &cmd, &pyargs, &syntax)) {
    PyErr_Format(PyExc_TypeError, "Invalid arguments to parse_args");
    return NULL;
  }

  // convert the character
  if(!PyChar_Check(pych) || (ch = PyChar_AsChar(pych)) == NULL) {
      PyErr_Format(PyExc_TypeError, 
		   "First argument must be an existent character!");
      return NULL;
  }

  // strdup our py args; they might be edited in the parse function
  parse_args = strdup(pyargs);

  // finish up and garbage collections
  PyObject *retval = Py_parse_args(ch, show_errors, cmd, parse_args, syntax);
  free(parse_args);
  return retval;
}


//
// expose C look functions to Python for consistent output and hooks
PyObject *mud_look_at_room(PyObject *self, PyObject *args) {
  PyObject *pych = NULL, *pyroom = NULL;
  if (!PyArg_ParseTuple(args, "OO", &pych, &pyroom)) {
    PyErr_Format(PyExc_TypeError, "look_at_room(ch, room)");
    return NULL;
  }
  CHAR_DATA *ch = NULL; ROOM_DATA *room = NULL;
  if(!PyChar_Check(pych) || (ch = PyChar_AsChar(pych)) == NULL) {
    PyErr_SetString(PyExc_TypeError, "First argument must be a character");
    return NULL;
  }
  if(!PyRoom_Check(pyroom) || (room = PyRoom_AsRoom(pyroom)) == NULL) {
    PyErr_SetString(PyExc_TypeError, "Second argument must be a room");
    return NULL;
  }
  look_at_room(ch, room);
  Py_RETURN_NONE;
}

PyObject *mud_look_at_obj(PyObject *self, PyObject *args) {
  PyObject *pych = NULL, *pyobj = NULL;
  if (!PyArg_ParseTuple(args, "OO", &pych, &pyobj)) {
    PyErr_Format(PyExc_TypeError, "look_at_obj(ch, obj)");
    return NULL;
  }
  CHAR_DATA *ch = NULL; OBJ_DATA *obj = NULL;
  if(!PyChar_Check(pych) || (ch = PyChar_AsChar(pych)) == NULL) {
    PyErr_SetString(PyExc_TypeError, "First argument must be a character");
    return NULL;
  }
  if(!PyObj_Check(pyobj) || (obj = PyObj_AsObj(pyobj)) == NULL) {
    PyErr_SetString(PyExc_TypeError, "Second argument must be an object");
    return NULL;
  }
  look_at_obj(ch, obj);
  Py_RETURN_NONE;
}

PyObject *mud_look_at_char(PyObject *self, PyObject *args) {
  PyObject *pych = NULL, *pyvict = NULL;
  if (!PyArg_ParseTuple(args, "OO", &pych, &pyvict)) {
    PyErr_Format(PyExc_TypeError, "look_at_char(ch, vict)");
    return NULL;
  }
  CHAR_DATA *ch = NULL; CHAR_DATA *vict = NULL;
  if(!PyChar_Check(pych) || (ch = PyChar_AsChar(pych)) == NULL) {
    PyErr_SetString(PyExc_TypeError, "First argument must be a character");
    return NULL;
  }
  if(!PyChar_Check(pyvict) || (vict = PyChar_AsChar(pyvict)) == NULL) {
    PyErr_SetString(PyExc_TypeError, "Second argument must be a character");
    return NULL;
  }
  look_at_char(ch, vict);
  Py_RETURN_NONE;
}

PyObject *mud_look_at_exit(PyObject *self, PyObject *args) {
  PyObject *pych = NULL, *pyex = NULL;
  if (!PyArg_ParseTuple(args, "OO", &pych, &pyex)) {
    PyErr_Format(PyExc_TypeError, "look_at_exit(ch, exit)");
    return NULL;
  }
  CHAR_DATA *ch = NULL; EXIT_DATA *ex = NULL;
  if(!PyChar_Check(pych) || (ch = PyChar_AsChar(pych)) == NULL) {
    PyErr_SetString(PyExc_TypeError, "First argument must be a character");
    return NULL;
  }
  if(!PyExit_Check(pyex) || (ex = PyExit_AsExit(pyex)) == NULL) {
    PyErr_SetString(PyExc_TypeError, "Second argument must be an exit");
    return NULL;
  }
  look_at_exit(ch, ex);
  Py_RETURN_NONE;
}


//
// a wrapper around NakedMud's generic_find() function
PyObject *mud_generic_find(PyObject *self, PyObject *args) {
  PyObject *py_looker = Py_None;     CHAR_DATA *looker = NULL;
  char      *type_str = NULL;        bitvector_t  type = 0; 
  char     *scope_str = NULL;        bitvector_t scope = 0;
  char           *arg = NULL;
  bool         all_ok = TRUE;

  // parse the arguments
  if(!PyArg_ParseTuple(args, "Osss|b", &py_looker, &arg, &type_str, &scope_str,
		       &all_ok)) {
    PyErr_Format(PyExc_TypeError,
		 "Invalid arguments supplied to mud.generic_find()");
    return NULL;
  }

  // convert the looker
  if(py_looker != Py_None) {
    if(!PyChar_Check(py_looker) || (looker = PyChar_AsChar(py_looker)) == NULL){
      PyErr_Format(PyExc_TypeError, 
		   "First argument must be an existent character or None!");
      return NULL;
    }
  }

  // convert the scope
  if(is_keyword(scope_str, "room", FALSE))
    SET_BIT(scope, FIND_SCOPE_ROOM);
  if(is_keyword(scope_str, "inv", FALSE))
    SET_BIT(scope, FIND_SCOPE_INV);
  if(is_keyword(scope_str, "worn", FALSE))
    SET_BIT(scope, FIND_SCOPE_WORN);
  if(is_keyword(scope_str, "world", FALSE))
    SET_BIT(scope, FIND_SCOPE_WORLD);
  if(is_keyword(scope_str, "visible", FALSE))
    SET_BIT(scope, FIND_SCOPE_VISIBLE);
  if(is_keyword(scope_str, "immediate", FALSE))
    SET_BIT(scope, FIND_SCOPE_IMMEDIATE);
  if(is_keyword(scope_str, "all", FALSE))
    SET_BIT(scope, FIND_SCOPE_ALL);

  // convert the types
  if(is_keyword(type_str, "obj", FALSE))
    SET_BIT(type, FIND_TYPE_OBJ);
  if(is_keyword(type_str, "char", FALSE))
    SET_BIT(type, FIND_TYPE_CHAR);
  if(is_keyword(type_str, "exit", FALSE))
    SET_BIT(type, FIND_TYPE_EXIT);
  if(is_keyword(type_str, "in", FALSE))
    SET_BIT(type, FIND_TYPE_IN_OBJ);
  if(is_keyword(type_str, "all", FALSE))
    SET_BIT(type,FIND_TYPE_OBJ  | FIND_TYPE_CHAR | 
	         FIND_TYPE_EXIT | FIND_TYPE_IN_OBJ);

  // do the search
  int found_type = FOUND_NONE;
  void    *found = generic_find(looker, arg, type, scope, all_ok, &found_type);

  if(found_type == FOUND_CHAR) {
    // were we searching for one type, or multiple types?
    if(!strcasecmp("char", type_str))
      return Py_BuildValue("O", charGetPyFormBorrowed(found));
    else
      return Py_BuildValue("Os", charGetPyFormBorrowed(found), "char");
  }
  else if(found_type == FOUND_EXIT) {
    // were we searching for one type, or multiple types?
    PyObject   *exit = newPyExit(found);
    PyObject *retval = NULL;
    if(!strcasecmp("exit", type_str))
      retval = Py_BuildValue("O", exit);
    else
      retval = Py_BuildValue("Os", exit, "obj");
    Py_DECREF(exit);
    return retval;
  }
  else if(found_type == FOUND_OBJ) {
    // were we searching for one type, or multiple types?
    if(!strcasecmp("obj", type_str))
      return Py_BuildValue("O", objGetPyFormBorrowed(found));
    else
      return Py_BuildValue("Os", objGetPyFormBorrowed(found), "obj");
  }
  else if(found_type == FOUND_IN_OBJ) {
    // were we searching for one type, or multiple types?
    if(!strcasecmp("in", type_str))
      return Py_BuildValue("O", objGetPyFormBorrowed(found));
    else
      return Py_BuildValue("Os", objGetPyFormBorrowed(found), "in");
  }
  // now it gets a bit more tricky... we have to see what other bit was set
  else if(found_type == FOUND_LIST) {
    PyObject         *list = PyList_New(0);
    LIST_ITERATOR *found_i = newListIterator(found);
    void        *one_found = NULL;
    if(IS_SET(type, FIND_TYPE_CHAR)) {
      ITERATE_LIST(one_found, found_i)
	PyList_Append(list, charGetPyFormBorrowed(one_found));
    }
    else if(IS_SET(type, FIND_TYPE_OBJ | FIND_TYPE_IN_OBJ)) {
      ITERATE_LIST(one_found, found_i)
	PyList_Append(list, objGetPyFormBorrowed(one_found));
    }
    deleteListIterator(found_i);
    deleteList(found);
    PyObject *retval = Py_BuildValue("Os", list, "list");
    Py_DECREF(list);
    return retval;
  }

  // nothing was found...
  return Py_BuildValue("OO", Py_None, Py_None);
}


//
// execute message() from inform.h
PyObject *mud_message(PyObject *self, PyObject *args) {
  // the python/C representations of the various variables that message() needs
  PyObject    *pych = NULL;     CHAR_DATA     *ch = NULL;
  PyObject  *pyvict = NULL;     CHAR_DATA   *vict = NULL;
  PyObject   *pyobj = NULL;     OBJ_DATA     *obj = NULL;
  PyObject  *pyvobj = NULL;     OBJ_DATA    *vobj = NULL;
  char     *pyrange = NULL;     bitvector_t range = 0;
  char        *mssg = NULL;
  int    hide_nosee = 0;

  // parse all of the arguments
  if(!PyArg_ParseTuple(args, "OOOObss", &pych, &pyvict, &pyobj, &pyvobj,
		       &hide_nosee, &pyrange, &mssg)) {
    PyErr_Format(PyExc_TypeError,"Invalid arguments supplied to mud.message()");
    return NULL;
  }

  // convert the character
  if(pych != Py_None) {
    if(!PyChar_Check(pych) || (ch = PyChar_AsChar(pych)) == NULL) {
      PyErr_Format(PyExc_TypeError, 
		   "First argument must be an existent character or None!");
      return NULL;
    }
  }

  // convert the victim
  if(pyvict != Py_None) {
    if(!PyChar_Check(pyvict) || (vict = PyChar_AsChar(pyvict)) == NULL) {
      PyErr_Format(PyExc_TypeError, 
		   "Second argument must be an existent character or None!");
      return NULL;
    }
  }

  // convert the object
  if(pyobj != Py_None) {
    if(!PyObj_Check(pyobj) || (obj = PyObj_AsObj(pyobj)) == NULL) {
      PyErr_Format(PyExc_TypeError, 
		   "Third argument must be an existent object or None!");
      return NULL;
    }
  }

  // convert the target object
  if(pyvobj != Py_None) {
    if(!PyObj_Check(pyvobj) || (vobj = PyObj_AsObj(pyvobj)) == NULL) {
      PyErr_Format(PyExc_TypeError, 
		   "Fourth argument must be an existent object or None!");
      return NULL;
    }
  }

  // check all of our keywords: char, vict, room
  if(is_keyword(pyrange, "to_char", FALSE))
    SET_BIT(range, TO_CHAR);
  if(is_keyword(pyrange, "to_vict", FALSE))
    SET_BIT(range, TO_VICT);
  if(is_keyword(pyrange, "to_room", FALSE))
    SET_BIT(range, TO_ROOM);
  if(is_keyword(pyrange, "to_world", FALSE))
    SET_BIT(range, TO_WORLD);

  // finally, send out the message
  message(ch, vict, obj, vobj, hide_nosee, range, mssg);
  return Py_BuildValue("i", 1);
}


//
// extracts an mob or object from the game
PyObject *mud_extract(PyObject *self, PyObject *args) {
  PyObject *thing = NULL;

  // parse the value
  if (!PyArg_ParseTuple(args, "O", &thing)) {
    PyErr_Format(PyExc_TypeError, 
		 "extract must be provided with an object or mob to extract!.");
    return NULL;
  }

  // check its type
  if(PyChar_Check(thing)) {
    CHAR_DATA *ch = PyChar_AsChar(thing);
    if(ch != NULL)
      extract_mobile(ch);
    else {
      PyErr_Format(PyExc_Exception,
		   "Tried to extract nonexistent character!");
      return NULL;
    }
  }
  else if(PyObj_Check(thing)) {
    OBJ_DATA *obj = PyObj_AsObj(thing);
    if(obj != NULL)
      extract_obj(obj);
    else {
      PyErr_Format(PyExc_Exception,
		   "Tried to extract nonexistent object!");
      return NULL;
    }
  }
  else if(PyRoom_Check(thing)) {
    ROOM_DATA *room = PyRoom_AsRoom(thing);
    if(room != NULL)
      extract_room(room);
    else {
      PyErr_Format(PyExc_Exception,
		   "Tried to extract nonexistent room!");
      return NULL;
    }
  }
  
  // success
  return Py_BuildValue("i", 1);
}

//
// functional form of if/then/else
PyObject *mud_ite(PyObject *self, PyObject *args) {
  PyObject *condition = NULL;
  PyObject  *true_act = NULL;
  PyObject *false_act = Py_None;

  if (!PyArg_ParseTuple(args, "OO|O", &condition, &true_act, &false_act)) {
    PyErr_Format(PyExc_TypeError, "ite must be specified 2 and an optional 3rd "
		 "arg");
    return NULL;
  }

  // check to see if our condition is true
  if( (PyLong_Check(condition)    && PyLong_AsLong(condition) != 0) ||
      (PyUnicode_Check(condition) && strlen(PyUnicode_AsUTF8(condition)) > 0))
    return true_act;
  else
    return false_act;
}

//
// returns whether or not two database keys have the same name. Locale sensitive
PyObject *mud_keys_equal(PyObject *self, PyObject *args) {
  char *key1 = NULL;
  char *key2 = NULL;
  if(!PyArg_ParseTuple(args, "ss", &key1, &key2)) {
    PyErr_Format(PyExc_TypeError, "keys_equal takes two string arguments");
    return NULL;
  }

  char *fullkey1 = strdup(get_fullkey_relative(key1, get_script_locale()));
  char *fullkey2 = strdup(get_fullkey_relative(key2, get_script_locale()));
  bool        ok = !strcasecmp(fullkey1, fullkey2);
  free(fullkey1);
  free(fullkey2);
  return Py_BuildValue("i", ok);
}

//
// returns the mud's message of the day
PyObject *mud_get_motd(PyObject *self, PyObject *args) {
  return Py_BuildValue("s", bufferString(motd));
}

//
// returns the mud's message of the day
PyObject *mud_get_greeting(PyObject *self, PyObject *args) {
  return Py_BuildValue("s", bufferString(greeting));
}

PyObject *mud_log_string(PyObject *self, PyObject *args) {
  char *mssg = NULL;
  if(!PyArg_ParseTuple(args, "s", &mssg)) {
    PyErr_Format(PyExc_TypeError, "a message must be supplied to log_string");
    return NULL;
  }

  // we have to strip all %'s out of this message
  BUFFER *buf = newBuffer(1);
  bufferCat(buf, mssg);
  bufferReplace(buf, "%", "%%", TRUE);
  log_string(bufferString(buf));
  deleteBuffer(buf);
  return Py_BuildValue("i", 1);
}

PyObject *mud_is_race(PyObject *self, PyObject *args) {
  char       *race = NULL;
  bool player_only = FALSE;
  if(!PyArg_ParseTuple(args, "s|b", &race, &player_only)) {
    PyErr_Format(PyExc_TypeError, "a string must be supplied");
    return NULL;
  }

  if(player_only)
    return Py_BuildValue("i", raceIsForPC(race));
  else
    return Py_BuildValue("i", isRace(race));
}

PyObject *mud_list_races(PyObject *self, PyObject *args) {
  bool player_only = FALSE;
  if(!PyArg_ParseTuple(args, "|b", &player_only)) {
    PyErr_Format(PyExc_TypeError, "true/false value to list only player races must be provided.");
    return NULL;
  }
  return Py_BuildValue("s", raceGetList(player_only));
}

PyObject *mud_send(PyObject *self, PyObject *args, PyObject *kwds) {
  static char *kwlist[ ] = { "list", "mssg", "dict", "newline", NULL };
  PyObject *list = NULL;
  char     *text = NULL;
  PyObject *dict = NULL;
  bool   newline = TRUE;

  if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os|Ob", kwlist, 
				  &list, &text, &dict, &newline)) {
    PyErr_Format(PyExc_TypeError, "Invalid arguments supplied to mud.send");
    return NULL;
  }

  // is dict None? set it to NULL for expand_to_char
  if(dict == Py_None)
    dict = NULL;

  // make sure the dictionary is a dictionary
  if(!(dict == NULL || PyDict_Check(dict))) {
    PyErr_Format(PyExc_TypeError, "mud.send expects third argument to be a dict object.");
    return NULL;
  }

  // make sure the list is a list
  if(!PyList_Check(list)) {
    PyErr_Format(PyExc_TypeError, "mud.send expects first argument to be a list of characters.");
    return NULL;
  }

  // go through our list of characters, and send each of them the message
  int i = 0;
  for(; i < PyList_Size(list); i++) {
    // make sure it's a character, and it has a socket
    PyObject *pych = PyList_GetItem(list, i);
    CHAR_DATA  *ch = NULL;
    if(!PyChar_Check(pych))
      continue;
    if( (ch = PyChar_AsChar(pych)) == NULL || charGetSocket(ch) == NULL)
      continue;
    if(dict != NULL)
      PyDict_SetItemString(dict, "ch", charGetPyFormBorrowed(ch));
    expand_to_char(ch, text, dict, get_script_locale(), newline);
  }
  return Py_BuildValue("");
}

PyObject *mud_expand_text(PyObject *self, PyObject *args, PyObject *kwds) {
  static char *kwlist[ ] = { "text", "dict", "newline", NULL };
  char     *text = NULL;
  PyObject *dict = NULL;
  bool   newline = FALSE;

  if(!PyArg_ParseTupleAndKeywords(args, kwds, "s|Ob", kwlist, 
				  &text, &dict, &newline)) {
    PyErr_Format(PyExc_TypeError, "Invalid arguments supplied to mud.expand_text");
    return NULL;
  }

  // is dict None? set it to NULL for expand_to_char
  if(dict == Py_None)
    dict = NULL;

  // make sure the dictionary is a dictionary
  if(!(dict == NULL || PyDict_Check(dict))) {
    PyErr_Format(PyExc_TypeError, "mud.expand_text expects second argument to be a dict object.");
    return NULL;
  }
  
  // build our script environment
  BUFFER   *buf = newBuffer(1);
  PyObject *env = restricted_script_dict();
  if(dict != NULL)
    PyDict_Update(env, dict);

  // do the expansion
  bufferCat(buf, text);
  expand_dynamic_descs_dict(buf, env, get_script_locale());
  if(newline == TRUE)
    bufferCat(buf, "\r\n");

  // garbage collection and return
  PyObject *ret = Py_BuildValue("s", bufferString(buf));
  Py_XDECREF(env);
  deleteBuffer(buf);
  return ret;
}



//*****************************************************************************
// MUD module
//*****************************************************************************/
void PyMud_addMethod(const char *name, void *f, int flags, const char *doc) {
  // make sure our list of methods is created
  if(pymud_methods == NULL) pymud_methods = newList();

  // make the Method def
  PyMethodDef *def = calloc(1, sizeof(PyMethodDef));
  def->ml_name     = strdup(name);
  def->ml_meth     = (PyCFunction)f;
  def->ml_flags    = flags;
  def->ml_doc      = (doc ? strdup(doc) : NULL);
  listPut(pymud_methods, def);
}


static struct PyModuleDef mud_moduledef = {
    PyModuleDef_HEAD_INIT,
    "mud",               // <-- should match what WAS in InitModule3
    "The mud module, for all MUD misc mud utils.", // <-- should match what WAS in InitModule3
    -1,                     // ?
    NULL,                   // makePyMethods(pymud_methods),  m_methods
    NULL,                   // m_slots
    NULL,                   // m_traverse
    NULL,                   // m_clear
    NULL                    // m_free
};

PyMODINIT_FUNC
PyInit_PyMud(void) {
  // add all of our methods
  PyMud_addMethod("get_global", mud_get_global, METH_VARARGS,
    "get_global(name)\n\n"
    "Return a non-persistent global variable, or None.");
  PyMud_addMethod("set_global", mud_set_global, METH_VARARGS,
    "set_global(name, val)\n\n"
    "Sets a non-persistent global variable. Val can be any type.");
  PyMud_addMethod("erase_global",  mud_erase_global, METH_VARARGS,
    "erase_global(name)\n\n"
    "Delete a value from the global variable table.");
  PyMud_addMethod("message", mud_message, METH_VARARGS,
    "message(ch, vict, obj, vobj, show_invis, range, mssg)\n\n"
    "Send a message via the mud messaging system using $ expansions. Range\n"
    "can be 'to_room', 'to_char', 'to_vict', or 'to_world'.");
  PyMud_addMethod("format_string", mud_format_string, METH_VARARGS,
    "format_string(text, indent=True, width=80)\n\n"
    "Format a block of text to be of the specified width, possibly indenting\n"
    "paragraphs.");
  PyMud_addMethod("look_at_room", mud_look_at_room, METH_VARARGS,
    "look_at_room(ch, room)\n\n"
    "Show the room description to the character with standard formatting and hooks.");
  PyMud_addMethod("look_at_obj", mud_look_at_obj, METH_VARARGS,
    "look_at_obj(ch, obj)\n\n"
    "Show the object description to the character with standard formatting and hooks.");
  PyMud_addMethod("look_at_char", mud_look_at_char, METH_VARARGS,
    "look_at_char(ch, vict)\n\n"
    "Show the character's description to the viewer with standard formatting and hooks.");
  PyMud_addMethod("look_at_exit", mud_look_at_exit, METH_VARARGS,
    "look_at_exit(ch, exit)\n\n"
    "Show the exit description to the character with standard formatting and hooks.");
  PyMud_addMethod("generic_find",  mud_generic_find, METH_VARARGS,
    "Deprecated. Use mud.parse_args instead.");
  PyMud_addMethod("extract", mud_extract, METH_VARARGS,
    "extract(thing)\n\n"
    "Extracts an object, character, or room from the game.");
  PyMud_addMethod("keys_equal", mud_keys_equal, METH_VARARGS,
    "keys_equal(key1, key2)\n\n"
    "Returns whether two world database keys are equal, relative to the\n"
    "locale (if any) that the current script is running in.");
  PyMud_addMethod("ite", mud_ite, METH_VARARGS,
    "ite(logic_statement, if_statement, else_statement=None)\n\n"
    "A functional form of if/then/else.");
  PyMud_addMethod("parse_args", mud_parse_args, METH_VARARGS,
    "parse_args(ch, show_usage_errors, cmd, args, format)\n\n"
    "equivalent to parse_args written in C. See parse.h for information.");
  PyMud_addMethod("get_motd", mud_get_motd, METH_NOARGS,
    "get_motd()\n\n"
    "Returns the mud's message of the day.");
  PyMud_addMethod("get_greeting", mud_get_greeting, METH_NOARGS,
    "get_greeting()\n\n"
    "returns the mud's connection greeting.");
  PyMud_addMethod("log_string", mud_log_string, METH_VARARGS,
    "log_string(mssg)\n"
    "Send a message to the mud's log.");
  PyMud_addMethod("is_race", mud_is_race, METH_VARARGS,
    "is_race(name)\n\n"
    "Returns True or False if the string is a valid race name.");
  PyMud_addMethod("list_races", mud_list_races, METH_VARARGS,
    "list_races(player_only=False)\n\n"
    "Return a list of available races. If player_only is True, list only the\n"
    "races that players have access to.");
  PyMud_addMethod("send", mud_send, METH_VARARGS | METH_KEYWORDS,
    "send(list, mssg, dict = None, newline = True)\n"
    "\n"
    "Sends a message to a list of characters. Messages can have scripts\n"
    "embedded in them, using [ and ]. If so, a variable dictionary must be\n"
    "provided. By default, 'ch' references each character being sent the\n"
    "message, for embedded scripts.");
  PyMud_addMethod("expand_text", mud_expand_text, METH_VARARGS | METH_KEYWORDS,
    "expand_text(text, dict={}, newline=False)\n\n"
    "Take text with embedded Python statements. Statements can be embedded\n"
    "between [ and ]. Expand them out and return the new text. Variables can\n"
    "be added to the scripting environment by specifying their names and\n"
    "values in an optional dictionary. Statements are expanded in the default\n"
    "scripting environment.");



  mud_moduledef.m_methods = makePyMethods(pymud_methods);

  PyObject *module = PyModule_Create(&mud_moduledef);

  if (module == NULL) {
       PyErr_Print();
       log_string("Failed to create mud module.\n");
    return NULL;

}

  globals = PyDict_New();
  Py_INCREF(globals);

  log_string("Succesfully returning the mud module.");
  return module;
}
