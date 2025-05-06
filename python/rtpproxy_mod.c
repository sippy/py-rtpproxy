#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#if defined(__linux__)
/* Get rid of annoying warning in cpython/pytime.h */
struct timespec;
extern char *strdup(const char *str);
#endif
#include <Python.h>
#include <structmember.h>

#include <librtpproxy.h>

#include "rtpproxy_mod.h"

#define MODULE_PREFIX rtp
#define MODULE_BASENAME io

#define CONCATENATE_DETAIL(x, y) x##y
#define CONCATENATE(x, y) CONCATENATE_DETAIL(x, y)

#if defined(DEBUG_MOD)
#define MODULE_BASENAME CONCATENATE(MODULE_BASENAME, _debug)
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define MODULE_NAME_STR TOSTRING(MODULE_PREFIX) "." TOSTRING(MODULE_BASENAME)
#define PY_INIT_FUNC CONCATENATE(PyInit_, MODULE_BASENAME)
#define CLS_NAME_STR "rtpproxy"

struct RTPPSocket {
    struct {
        int our; // Raw fd
        int their;
    } fds;
    struct {
      PyObject *rtpp_sock; // Strong reference to Python socket object
      PyObject *spec_str;  // Python string object for rtpp_spec
    } py;
    char rtpp_spec[16];
};

typedef struct {
    PyObject_HEAD
    struct rtpp_cfg *rtpp_ctx;
    struct RTPPSocket cmd;
    struct RTPPSocket notify;
    struct RTPPInitializeParams rp;
    char ttl_val[16];
    char setup_ttl_val[16];
    char port_min[16];
    char port_max[16];
    const char *_modules[MAX_MODULES + 1];
    const char *_extra_args[MAX_EXTRA_ARGS + 1];
} PyRTPProxyObject;

static const char *default_modules[] = {
    "acct_csv", "catch_dtmf", "dtls_gw", "ice_lite", NULL
};

static const struct RTPPInitializeParams RTPPInitializeParams = {
    .ttl = -1,
    .setup_ttl = -1,
    .socket = NULL,
    .debug_level = "info",
    .notify_socket = "tcp:127.0.0.1:9642",
    .rec_spool_dir = "/tmp",
    .rec_final_dir = ".",
    .modules = default_modules,
};

static struct RTPPSocket
getRTPPSocket()
{
    int fds[2];
    struct RTPPSocket r = {0};

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
        PyErr_SetFromErrno(PyExc_OSError);
        goto e0;
    }
    PyObject *sock_obj = PyImport_ImportModule("socket");
    if (sock_obj == NULL)
        goto e1;
    PyObject *sock_class = PyObject_GetAttrString(sock_obj, "socket");
    Py_DECREF(sock_obj);
    if (sock_class == NULL)
        goto e1;
    PyObject *fd_arg = PyLong_FromLong(fds[0]);
    if (fd_arg == NULL)
        goto e2;
    PyObject *kwargs = Py_BuildValue("{s:O}", "fileno", fd_arg);
    if (kwargs == NULL)
        goto e3;
    snprintf(r.rtpp_spec, sizeof(r.rtpp_spec), "fd:%d", fds[1]);
    r.py.spec_str = PyUnicode_FromString(r.rtpp_spec);
    if (r.py.spec_str == NULL)
        goto e4;
    r.py.rtpp_sock = PyObject_Call(sock_class, PyTuple_New(0), kwargs);
    if (r.py.rtpp_sock == NULL) {
        goto e5;
    }
    r.fds.our = fds[0];
    r.fds.their = fds[1];
    Py_DECREF(kwargs);
    Py_DECREF(fd_arg);
    Py_DECREF(sock_class);
    return r;
e5:
    Py_DECREF(r.py.spec_str);
e4:
    Py_DECREF(kwargs);
e3:
    Py_DECREF(fd_arg);
e2:
    Py_DECREF(sock_class);
e1:
    close(fds[0]);
    close(fds[1]);
e0:
    return r;
}

static int
arg_count(const char *argv[]) {
    int i;
    for (i = 0; argv[i] != NULL; i++) {}
    return i;
}

static void
arg_append1(const char *argv[], const char *arg) {
    int i = arg_count(argv);
    argv[i++] = arg;
    argv[i] = NULL;
}

static void
arg_append2(const char *argv[], const char *arg, const char *val) {
    int i = arg_count(argv);
    argv[i++] = arg;
    argv[i++] = val;
    argv[i] = NULL;
}

static int
py2c_list(PyObject *list, const char **c_list, int max, const char *name) {
    Py_ssize_t mcount = PySequence_Size(list);
    const char *errf;
    char emsg[256];
    PyObject *erro = PyExc_TypeError;

    if (mcount > max) {
        errf = "Too many %s specified";
        goto e0;
    }
    if (!PySequence_Check(list)) {
        errf = "%s must be a list";
        goto e0;
    }
    int i;
    for (i = 0; i < mcount; i++) {
        PyObject *item = PySequence_GetItem(list, i);
        if (!PyUnicode_Check(item)) {
            Py_XDECREF(item);
            errf = "%s must be a list of strings";
            goto e1;
        }
        c_list[i] = strdup(PyUnicode_AsUTF8(item));  // Does not increase refcount
        Py_DECREF(item);
        if (c_list[i] == NULL) {
            erro = PyExc_MemoryError;
            errf = "Failed to allocate memory for %s names";
            goto e1;
        }
    }
    return 0;
e1:
    for (int j = 0; j < i; j++)
        free((char *)c_list[j]);
e0:
    snprintf(emsg, sizeof(emsg), errf, name);
    PyErr_SetString(erro, emsg);
    return -1;
}

static int PyRTPProxy_init(PyRTPProxyObject* self, PyObject* args, PyObject* kwds) {
    static const char *kwlist[] = {
        "ttl", "setup_ttl", "debug_level", "port_min", "port_max",
        "rec_spool_dir", "rec_final_dir", "modules", "extra_args",
        NULL
    };
    self->rp = RTPPInitializeParams;
    PyObject *modules_obj = NULL;
    PyObject *extra_args_obj = NULL;
    if (!PyArg_ParseTupleAndKeywords(args, kwds, "|iiiisssOO", (char **)kwlist,
            &self->rp.ttl, &self->rp.setup_ttl, &self->rp.port_min, &self->rp.port_max,
            &self->rp.debug_level, &self->rp.rec_spool_dir, &self->rp.rec_final_dir,
            &modules_obj, &extra_args_obj)) {
        goto e0;
    }

    if (modules_obj) {
        if (py2c_list(modules_obj, self->_modules, MAX_MODULES, "modules") != 0) {
            goto e0;
        }
        self->rp.modules = self->_modules;
    }
    if (extra_args_obj) {
        if (py2c_list(extra_args_obj, self->_extra_args, MAX_EXTRA_ARGS, "extra_args") != 0) {
            goto e1;
        }
    }

    self->cmd = getRTPPSocket();
    if (self->cmd.py.rtpp_sock == NULL)
        goto e2;
    self->notify = getRTPPSocket();
    if (self->notify.py.rtpp_sock == NULL)
        goto e3;

    if (self->rp.debug_level != RTPPInitializeParams.debug_level)
        self->rp.debug_level = strdup(self->rp.debug_level);
    if (self->rp.rec_spool_dir != RTPPInitializeParams.rec_spool_dir)
        self->rp.rec_spool_dir = strdup(self->rp.rec_spool_dir);
    if (self->rp.rec_final_dir != RTPPInitializeParams.rec_final_dir)
        self->rp.rec_final_dir = strdup(self->rp.rec_final_dir);
    if (self->rp.debug_level == NULL || self->rp.rec_spool_dir == NULL ||
      self->rp.rec_final_dir == NULL) {
        PyErr_SetString(PyExc_ValueError, "Failed to allocate memory for module values");
        goto e4;
    }
    const char *argv[256] = {
       "rtpproxy",
       "-s", self->cmd.rtpp_spec,
       "-d", self->rp.debug_level,
       "-n", self->notify.rtpp_spec,
       "-S", self->rp.rec_spool_dir,
       "-r", self->rp.rec_final_dir,
    };
    if (self->rp.ttl >= 0) {
        snprintf(self->ttl_val, sizeof(self->ttl_val), "%d", self->rp.ttl);
        arg_append2(argv, "-T", self->ttl_val);
    }
    if (self->rp.setup_ttl >= 0) {
        snprintf(self->setup_ttl_val, sizeof(self->setup_ttl_val), "%d",
          self->rp.setup_ttl);
        arg_append2(argv, "-W", self->setup_ttl_val);
    }
    for (int i = 0; i < MAX_MODULES; i++) {
        if (self->rp.modules[i] == NULL)
            break;
        arg_append2(argv, "--dso", self->rp.modules[i]);
    }
    if (self->rp.port_min > 0) {
        snprintf(self->port_min, sizeof(self->port_min), "%d", self->rp.port_min);
        arg_append2(argv, "-m", self->port_min);
    }
    if (self->rp.port_max > 0) {
        snprintf(self->port_max, sizeof(self->port_max), "%d", self->rp.port_max);
        arg_append2(argv, "-M", self->port_max);
    }
    for (int i = 0; i < MAX_EXTRA_ARGS; i++) {
        if (self->_extra_args[i] == NULL)
            break;
        arg_append1(argv, self->_extra_args[i]);
    }
    int argc = arg_count(argv);

    self->rtpp_ctx = rtpp_main(argc, argv);
    if(self->rtpp_ctx == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Error initializing RTPProxy instance");
        goto e4;
    }

    return 0;
e4:
    if (self->rp.debug_level != RTPPInitializeParams.debug_level)
        free((char *)self->rp.debug_level);
    if (self->rp.rec_spool_dir != RTPPInitializeParams.rec_spool_dir)
        free((char *)self->rp.rec_spool_dir);
    if (self->rp.rec_final_dir != RTPPInitializeParams.rec_final_dir)
        free((char *)self->rp.rec_final_dir);
    close(self->notify.fds.their);
    Py_DECREF(self->notify.py.rtpp_sock);
    Py_DECREF(self->notify.py.spec_str);
e3:
    close(self->cmd.fds.their);
    Py_DECREF(self->cmd.py.rtpp_sock);
    Py_DECREF(self->cmd.py.spec_str);
e2:
    for (int i = 0; i < MAX_EXTRA_ARGS && self->_extra_args[i] != NULL; i++)
        free((char *)self->_extra_args[i]);
e1:
    for (int i = 0; i < MAX_MODULES && self->_modules[i] != NULL; i++)
        free((char *)self->_modules[i]);
e0:
    return -1;
}

// The __del__ method for PyRTPProxy objects
static void PyRTPProxy_dealloc(PyRTPProxyObject* self) {
    if (self->rtpp_ctx != NULL) {
        rtpp_shutdown(self->rtpp_ctx);
        Py_DECREF(self->cmd.py.rtpp_sock);
        Py_DECREF(self->cmd.py.spec_str);
        close(self->cmd.fds.their);
        Py_DECREF(self->notify.py.rtpp_sock);
        Py_DECREF(self->notify.py.spec_str);
        close(self->notify.fds.their);
        if (self->rp.debug_level != RTPPInitializeParams.debug_level)
            free((char *)self->rp.debug_level);
        if (self->rp.rec_spool_dir != RTPPInitializeParams.rec_spool_dir)
            free((char *)self->rp.rec_spool_dir);
        if (self->rp.rec_final_dir != RTPPInitializeParams.rec_final_dir)
            free((char *)self->rp.rec_final_dir);
        for (int i = 0; i < MAX_MODULES && self->_modules[i] != NULL; i++) {
            free((char *)self->_modules[i]);
        }
    }
}

static PyMethodDef PyRTPProxy_methods[] = {
    {NULL}  // Sentinel
};

static PyMemberDef PyRTPProxy_members[] = {
    {"rtpp_sock", T_OBJECT_EX, offsetof(PyRTPProxyObject, cmd.py.rtpp_sock),
     READONLY, "RTPProxy command socket"},
    {"rtpp_nsock", T_OBJECT_EX, offsetof(PyRTPProxyObject, notify.py.rtpp_sock),
     READONLY, "RTPProxy notification socket"},
    {"rtpp_sock_spec", T_OBJECT_EX, offsetof(PyRTPProxyObject, cmd.py.spec_str),
     READONLY, "RTPProxy command socket specifier"},
    {"rtpp_nsock_spec", T_OBJECT_EX, offsetof(PyRTPProxyObject, notify.py.spec_str),
     READONLY, "RTPProxy notification socket specifier"},
    {NULL}
};

static PyTypeObject PyRTPProxyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = MODULE_NAME_STR "." CLS_NAME_STR,
    .tp_doc = "Module to run RTPProxy inside Python.",
    .tp_basicsize = sizeof(PyRTPProxyObject),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc)PyRTPProxy_init,
    .tp_dealloc = (destructor)PyRTPProxy_dealloc,
    .tp_methods = PyRTPProxy_methods,
    .tp_members = PyRTPProxy_members,
};

static struct PyModuleDef RTPProxy_module = {
    PyModuleDef_HEAD_INIT,
    .m_name = MODULE_NAME_STR,
    .m_doc = "Module to run RTPProxy inside Python.",
    .m_size = -1,
};

// Module initialization function
PyMODINIT_FUNC PY_INIT_FUNC(void) {
    PyObject* module;
    if (PyType_Ready(&PyRTPProxyType) < 0)
        return NULL;

    module = PyModule_Create(&RTPProxy_module);
    if (module == NULL)
        return NULL;

    Py_INCREF(&PyRTPProxyType);
    PyModule_AddObject(module, CLS_NAME_STR, (PyObject*)&PyRTPProxyType);

    return module;
}
