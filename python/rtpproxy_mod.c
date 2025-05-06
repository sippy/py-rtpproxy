#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#if defined(__linux__)
/* Get rid of annoying warning in cpython/pytime.h */
struct timespec;
#endif
#include <Python.h>

#include <librtpproxy.h>

#include "rtpproxy_mod.h"

#define MODULE_BASENAME rtpproxy

#define CONCATENATE_DETAIL(x, y) x##y
#define CONCATENATE(x, y) CONCATENATE_DETAIL(x, y)

#if !defined(DEBUG_MOD)
#define MODULE_NAME MODULE_BASENAME
#else
#define MODULE_NAME CONCATENATE(MODULE_BASENAME, _debug)
#endif

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define MODULE_NAME_STR TOSTRING(MODULE_NAME)
#define PY_INIT_FUNC CONCATENATE(PyInit_, MODULE_NAME)

typedef struct {
    PyObject_HEAD
    struct rtpp_cfg *rtpp_ctx;
} PyRTPProxy;

static const struct RTPPInitializeParams RTPPInitializeParams = {
    .ttl = "1",
    .setup_ttl = "1",
    .socket = NULL,
    .debug_level = "crit",
    .notify_socket = "tcp:127.0.0.1:9642",
    .rec_spool_dir = "/tmp",
    .rec_final_dir = ".",
    .modules = {"acct_csv", "catch_dtmf", "dtls_gw", "ice_lite", NULL},
};

static int PyRTPProxy_init(PyRTPProxy* self, PyObject* args, PyObject* kwds) {
    const struct RTPPInitializeParams *rp = &RTPPInitializeParams;
    const char *argv[] = {
       "rtpproxy",
       "-f",
       "-T", rp->ttl,
       "-W", rp->setup_ttl,
       "-s", (rp->socket != NULL) ? rp->socket : tmpnam(NULL),
       "-d", rp->debug_level,
       "-n", rp->notify_socket,
       "-S", rp->rec_spool_dir,
       "-r", rp->rec_final_dir,
       "--dso", rp->modules[0],
       "--dso", rp->modules[1],
       "--dso", rp->modules[2],
       "--dso", rp->modules[3],
    };
    int argc = howmany(argv, *argv);

    self->rtpp_ctx = rtpp_main(argc, argv);
    if(self->rtpp_ctx == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Error initializing RTPProxy instance");
        return -1;
    }

    return 0;
}

// The __del__ method for PyRTPProxy objects
static void PyRTPProxy_dealloc(PyRTPProxy* self) {
    if (self->rtpp_ctx != NULL)
        rtpp_shutdown(self->rtpp_ctx);
}

static PyMethodDef PyRTPProxy_methods[] = {
    {NULL}  // Sentinel
};

static PyTypeObject PyRTPProxyType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = MODULE_NAME_STR "." MODULE_NAME_STR,
    .tp_doc = "Module to run RTPProxy inside Python.",
    .tp_basicsize = sizeof(PyRTPProxy),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc)PyRTPProxy_init,
    .tp_dealloc = (destructor)PyRTPProxy_dealloc,
    .tp_methods = PyRTPProxy_methods,
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
    PyModule_AddObject(module, MODULE_NAME_STR, (PyObject*)&PyRTPProxyType);

    return module;
}

