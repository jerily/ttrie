#include <tcl.h>
#include <string.h>
#include "ttrie.h"

#if TCL_MAJOR_VERSION < 9
typedef int Tcl_Size;
#endif

static int error(Tcl_Interp *interp, const char *message) {
    Tcl_SetObjResult(interp, Tcl_NewStringObj(message, -1));
    return TCL_ERROR;
}

static const char *get_key(Tcl_Interp *interp, Tcl_Obj *object) {
    Tcl_Size length;
    const char *key = Tcl_GetStringFromObj(object, &length);
    if (length < 1 || length > TTRIE_MAX_LENGTH || memchr(key, '\0', (size_t)length)) {
        error(interp, "key must contain 1 to 255 non-NUL UTF-8 bytes");
        return NULL;
    }
    // Tcl 8.6 represents U+0000 using modified UTF-8.
    for (Tcl_Size i = 0; i + 1 < length; ++i) {
        if ((unsigned char)key[i] == 0xc0 && (unsigned char)key[i + 1] == 0x80) {
            error(interp, "key must contain 1 to 255 non-NUL UTF-8 bytes");
            return NULL;
        }
    }
    return key;
}

static void delete_trie(void *data) { ttrie_destroy(data); }

static int append_match(const char *key, int distance, void *context) {
    Tcl_Obj *fields[] = {Tcl_NewStringObj("name", -1), Tcl_NewStringObj(key, -1),
                         Tcl_NewStringObj("distance", -1), Tcl_NewIntObj(distance)};
    return Tcl_ListObjAppendElement(NULL, context, Tcl_NewListObj(4, fields));
}

static int trie_command(void *data, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[]) {
    static const char *operations[] = {"insert", "contains", "search", "destroy", NULL};
    enum {INSERT, CONTAINS, SEARCH, DESTROY};
    int operation;
    if (objc < 2) {
        Tcl_WrongNumArgs(interp, 1, objv, "insert|contains|search|destroy ?args?");
        return TCL_ERROR;
    }
    if (Tcl_GetIndexFromObj(interp, objv[1], operations, "operation", TCL_EXACT, &operation) != TCL_OK) {
        return TCL_ERROR;
    }
    if (operation == DESTROY) {
        if (objc != 2) { Tcl_WrongNumArgs(interp, 2, objv, NULL); return TCL_ERROR; }
        return Tcl_DeleteCommand(interp, Tcl_GetString(objv[0]));
    }
    if (objc != 3 && !(operation == SEARCH && objc == 4)) {
        Tcl_WrongNumArgs(interp, 2, objv, operation == SEARCH ? "query ?max_distance?" : "key");
        return TCL_ERROR;
    }
    const char *key = get_key(interp, objv[2]);
    if (!key) { return TCL_ERROR; }
    if (operation == INSERT) {
        return ttrie_insert(data, key) == 0 ? TCL_OK : error(interp, "cannot allocate trie entry");
    }
    if (operation == CONTAINS) {
        Tcl_SetObjResult(interp, Tcl_NewBooleanObj(ttrie_contains(data, key)));
        return TCL_OK;
    }
    int distance = TTRIE_MAX_DISTANCE;
    if (objc == 4 && Tcl_GetIntFromObj(interp, objv[3], &distance) != TCL_OK) { return TCL_ERROR; }
    if (distance < 0 || distance > TTRIE_MAX_DISTANCE) {
        return error(interp, "max_distance must be 0, 1 or 2");
    }
    Tcl_Obj *matches = Tcl_NewListObj(0, NULL);
    Tcl_IncrRefCount(matches);
    int status = ttrie_search(data, key, distance, append_match, matches);
    if (!status) { Tcl_SetObjResult(interp, matches); }
    Tcl_DecrRefCount(matches);
    return status ? error(interp, "trie search failed") : TCL_OK;
}

static int create_command(void *data, Tcl_Interp *interp, Tcl_Size objc, Tcl_Obj *const objv[]) {
    (void)data;
    if (objc != 2 && objc != 3) {
        Tcl_WrongNumArgs(interp, 1, objv, "command_name ?keys?");
        return TCL_ERROR;
    }
    const char *name = get_key(interp, objv[1]);
    if (!name) { return TCL_ERROR; }
    Tcl_CmdInfo info;
    if (Tcl_GetCommandInfo(interp, name, &info)) {
        return error(interp, "command already exists");
    }
    Tcl_Size count = 0;
    Tcl_Obj **keys = NULL;
    if (objc == 3 && Tcl_ListObjGetElements(interp, objv[2], &count, &keys) != TCL_OK) { return TCL_ERROR; }
    ttrie *trie = ttrie_create();
    if (!trie) { return error(interp, "cannot allocate trie"); }
    for (Tcl_Size i = 0; i < count; ++i) {
        const char *key = get_key(interp, keys[i]);
        if (!key || ttrie_insert(trie, key) != 0) {
            ttrie_destroy(trie);
            return key ? error(interp, "cannot allocate trie entry") : TCL_ERROR;
        }
    }
    // Install only after every key has been validated and inserted.
    Tcl_Command command = Tcl_CreateObjCommand(interp, name, trie_command, trie, delete_trie);
    if (!command) { ttrie_destroy(trie); return TCL_ERROR; }
    Tcl_Obj *full_name = Tcl_NewObj();
    Tcl_GetCommandFullName(interp, command, full_name);
    Tcl_SetObjResult(interp, full_name);
    return TCL_OK;
}

DLLEXPORT int Ttrie_Init(Tcl_Interp *interp) {
    if (!Tcl_InitStubs(interp, TCL_VERSION, 0)) { return TCL_ERROR; }
    if (!Tcl_FindNamespace(interp, "::ttrie", NULL, 0) &&
        !Tcl_CreateNamespace(interp, "::ttrie", NULL, NULL)) { return TCL_ERROR; }
    Tcl_CreateObjCommand(interp, "::ttrie::create", create_command, NULL, NULL);
    return Tcl_PkgProvide(interp, "ttrie", TTRIE_VERSION);
}
