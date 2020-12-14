/* Iterator objects */

#include "Python.h"
#include "pycore_object.h"

typedef struct {
    PyObject_HEAD
    Py_ssize_t it_index;
    PyObject *it_seq; /* Set to NULL when iterator is exhausted */
} seqiterobject;

_Py_IDENTIFIER(iter);

PyObject *
PySeqIter_New(PyObject *seq)
{
    seqiterobject *it;

    if (!PySequence_Check(seq)) {
        PyErr_BadInternalCall();
        return NULL;
    }
    it = PyObject_GC_New(seqiterobject, &PySeqIter_Type);
    if (it == NULL)
        return NULL;
    it->it_index = 0;
    Py_INCREF(seq);
    it->it_seq = seq;
    _PyObject_GC_TRACK(it);
    return (PyObject *)it;
}

static void
iter_dealloc(seqiterobject *it)
{
    _PyObject_GC_UNTRACK(it);
    Py_XDECREF(it->it_seq);
    PyObject_GC_Del(it);
}

static int
iter_traverse(seqiterobject *it, visitproc visit, void *arg)
{
    Py_VISIT(it->it_seq);
    return 0;
}

static PyObject *
iter_iternext(PyObject *iterator)
{
    seqiterobject *it;
    PyObject *seq;
    PyObject *result;

    assert(PySeqIter_Check(iterator));
    it = (seqiterobject *)iterator;
    seq = it->it_seq;
    if (seq == NULL)
        return NULL;
    if (it->it_index == PY_SSIZE_T_MAX) {
        PyErr_SetString(PyExc_OverflowError,
                        "iter index too large");
        return NULL;
    }

    result = PySequence_GetItem(seq, it->it_index);
    if (result != NULL) {
        it->it_index++;
        return result;
    }
    if (PyErr_ExceptionMatches(PyExc_IndexError) ||
        PyErr_ExceptionMatches(PyExc_StopIteration))
    {
        PyErr_Clear();
        it->it_seq = NULL;
        Py_DECREF(seq);
    }
    return NULL;
}

static PyObject *
iter_len(seqiterobject *it, PyObject *Py_UNUSED(ignored))
{
    Py_ssize_t seqsize, len;

    if (it->it_seq) {
        if (_PyObject_HasLen(it->it_seq)) {
            seqsize = PySequence_Size(it->it_seq);
            if (seqsize == -1)
                return NULL;
        }
        else {
            Py_RETURN_NOTIMPLEMENTED;
        }
        len = seqsize - it->it_index;
        if (len >= 0)
            return PyLong_FromSsize_t(len);
    }
    return PyLong_FromLong(0);
}

PyDoc_STRVAR(length_hint_doc, "Private method returning an estimate of len(list(it)).");

static PyObject *
iter_reduce(seqiterobject *it, PyObject *Py_UNUSED(ignored))
{
    if (it->it_seq != NULL)
        return Py_BuildValue("N(O)n", _PyEval_GetBuiltinId(&PyId_iter),
                             it->it_seq, it->it_index);
    else
        return Py_BuildValue("N(())", _PyEval_GetBuiltinId(&PyId_iter));
}

PyDoc_STRVAR(reduce_doc, "Return state information for pickling.");

static PyObject *
iter_setstate(seqiterobject *it, PyObject *state)
{
    Py_ssize_t index = PyLong_AsSsize_t(state);
    if (index == -1 && PyErr_Occurred())
        return NULL;
    if (it->it_seq != NULL) {
        if (index < 0)
            index = 0;
        it->it_index = index;
    }
    Py_RETURN_NONE;
}

PyDoc_STRVAR(setstate_doc, "Set state information for unpickling.");

static PyMethodDef seqiter_methods[] = {
    {"__length_hint__", (PyCFunction)iter_len, METH_NOARGS, length_hint_doc},
    {"__reduce__", (PyCFunction)iter_reduce, METH_NOARGS, reduce_doc},
    {"__setstate__", (PyCFunction)iter_setstate, METH_O, setstate_doc},
    {NULL,              NULL}           /* sentinel */
};

PyTypeObject PySeqIter_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "iterator",                                 /* tp_name */
    sizeof(seqiterobject),                      /* tp_basicsize */
    0,                                          /* tp_itemsize */
    /* methods */
    (destructor)iter_dealloc,                   /* tp_dealloc */
    0,                                          /* tp_vectorcall_offset */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_as_async */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,/* tp_flags */
    0,                                          /* tp_doc */
    (traverseproc)iter_traverse,                /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    PyObject_SelfIter,                          /* tp_iter */
    iter_iternext,                              /* tp_iternext */
    seqiter_methods,                            /* tp_methods */
    0,                                          /* tp_members */
};

/* -------------------------------------- */

typedef struct {
    PyObject_HEAD
    PyObject *it_callable; /* Set to NULL when iterator is exhausted */
    PyObject *it_sentinel; /* Set to NULL when iterator is exhausted */
} calliterobject;

PyObject *
PyCallIter_New(PyObject *callable, PyObject *sentinel)
{
    calliterobject *it;
    it = PyObject_GC_New(calliterobject, &PyCallIter_Type);
    if (it == NULL)
        return NULL;
    Py_INCREF(callable);
    it->it_callable = callable;
    Py_INCREF(sentinel);
    it->it_sentinel = sentinel;
    _PyObject_GC_TRACK(it);
    return (PyObject *)it;
}
static void
calliter_dealloc(calliterobject *it)
{
    _PyObject_GC_UNTRACK(it);
    Py_XDECREF(it->it_callable);
    Py_XDECREF(it->it_sentinel);
    PyObject_GC_Del(it);
}

static int
calliter_traverse(calliterobject *it, visitproc visit, void *arg)
{
    Py_VISIT(it->it_callable);
    Py_VISIT(it->it_sentinel);
    return 0;
}

static PyObject *
calliter_iternext(calliterobject *it)
{
    PyObject *result;

    if (it->it_callable == NULL) {
        return NULL;
    }

    result = _PyObject_CallNoArg(it->it_callable);
    if (result != NULL) {
        int ok;

        ok = PyObject_RichCompareBool(it->it_sentinel, result, Py_EQ);
        if (ok == 0) {
            return result; /* Common case, fast path */
        }

        Py_DECREF(result);
        if (ok > 0) {
            Py_CLEAR(it->it_callable);
            Py_CLEAR(it->it_sentinel);
        }
    }
    else if (PyErr_ExceptionMatches(PyExc_StopIteration)) {
        PyErr_Clear();
        Py_CLEAR(it->it_callable);
        Py_CLEAR(it->it_sentinel);
    }
    return NULL;
}

static PyObject *
calliter_reduce(calliterobject *it, PyObject *Py_UNUSED(ignored))
{
    if (it->it_callable != NULL && it->it_sentinel != NULL)
        return Py_BuildValue("N(OO)", _PyEval_GetBuiltinId(&PyId_iter),
                             it->it_callable, it->it_sentinel);
    else
        return Py_BuildValue("N(())", _PyEval_GetBuiltinId(&PyId_iter));
}

static PyMethodDef calliter_methods[] = {
    {"__reduce__", (PyCFunction)calliter_reduce, METH_NOARGS, reduce_doc},
    {NULL,              NULL}           /* sentinel */
};

PyTypeObject PyCallIter_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "callable_iterator",                        /* tp_name */
    sizeof(calliterobject),                     /* tp_basicsize */
    0,                                          /* tp_itemsize */
    /* methods */
    (destructor)calliter_dealloc,               /* tp_dealloc */
    0,                                          /* tp_vectorcall_offset */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    0,                                          /* tp_as_async */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,/* tp_flags */
    0,                                          /* tp_doc */
    (traverseproc)calliter_traverse,            /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    PyObject_SelfIter,                          /* tp_iter */
    (iternextfunc)calliter_iternext,            /* tp_iternext */
    calliter_methods,                           /* tp_methods */
};


/* -------------------------------------- */

typedef struct {
    PyObject_HEAD
    PyObject *it_callable; /* Set to NULL when iterator is exhausted */
    PyObject *it_sentinel; /* Set to NULL when iterator is exhausted */
} asynccalliterobject;

typedef struct {
    PyObject_HEAD
    PyObject *wrapped_iter; /*
        The iterator returned by the callable, unwrapped via __await__.

        If NULL this means that the iterator is already exhausted and when
        iterated it should raise StopAsyncIteration;
    */
    asynccalliterobject *it; /* The iterator object, in order to clear it when done. */
} asynccallawaitableobject;

typedef struct {
    PyObject_HEAD
    PyObject *wrapped;
    PyObject *default_value;
} anextobject;

PyObject *
PyCallAiter_New(PyObject *callable, PyObject *sentinel)
{
    asynccalliterobject *it;
    it = PyObject_GC_New(asynccalliterobject, &PyCallAiter_Type);
    if (it == NULL)
        return NULL;
    Py_INCREF(callable);
    it->it_callable = callable;
    Py_INCREF(sentinel);
    it->it_sentinel = sentinel;
    _PyObject_GC_TRACK(it);
    return (PyObject *)it;
}

PyObject *
PyCallAnext_New(PyObject *awaitable, PyObject *default_value)
{
    anextobject *anext = PyObject_GC_New(anextobject, &PyAnext_Type);
    Py_INCREF(awaitable);
    anext->wrapped = awaitable;
    Py_INCREF(default_value);
    anext->default_value = default_value;
    _PyObject_GC_TRACK(anext);
    return (PyObject *)anext;
}

static void
asynccalliter_dealloc(asynccalliterobject *it)
{
    _PyObject_GC_UNTRACK(it);
    Py_XDECREF(it->it_callable);
    Py_XDECREF(it->it_sentinel);
    PyObject_GC_Del(it);
}

static void
asynccallawaitable_dealloc(asynccallawaitableobject *obj)
{
    _PyObject_GC_UNTRACK(obj);
    Py_XDECREF(obj->wrapped_iter);
    Py_XDECREF(obj->it);
    PyObject_GC_Del(obj);
}

static void
anext_dealloc(anextobject *obj)
{
    _PyObject_GC_UNTRACK(obj);
    Py_XDECREF(obj->wrapped);
    Py_XDECREF(obj->default_value);
    PyObject_GC_Del(obj);
}

static int
asynccalliter_traverse(asynccalliterobject *it, visitproc visit, void *arg)
{
    Py_VISIT(it->it_callable);
    Py_VISIT(it->it_sentinel);
    return 0;
}

static int
asynccallawaitable_traverse(asynccallawaitableobject *obj, visitproc visit, void *arg)
{
    Py_VISIT(obj->wrapped_iter);
    Py_VISIT(obj->it);
    return 0;
}

static int
anext_traverse(anextobject *obj, visitproc visit, void *arg)
{
    Py_VISIT(obj->wrapped);
    Py_VISIT(obj->default_value);
    return 0;
}

static PyObject *
asynccalliter_anext(asynccalliterobject *it)
{
    PyObject *obj = NULL, *wrapped_iter = NULL;
    PyTypeObject *t = NULL;
    asynccallawaitableobject *awaitable;

    if (it->it_callable == NULL) {
        /* Can we raise this at this point, or do we need to return an awaitable
         * that raises it?
         */
        PyObject *value = _PyObject_New((PyTypeObject *) PyExc_StopAsyncIteration);
        if (value == NULL) {
            return NULL;
        }
        PyErr_SetObject(PyExc_StopAsyncIteration, value);
        return NULL;
    }

    obj = _PyObject_CallNoArg(it->it_callable);
    if (obj == NULL) {
        return NULL;
    }

    t = Py_TYPE(obj);
    if (t->tp_as_async == NULL || t->tp_as_async->am_await == NULL) {
        return PyErr_Format(PyExc_TypeError, "'%.200s' object is not awaitable", obj);
    }
    
    wrapped_iter = (*t->tp_as_async->am_await)(obj);

    awaitable = PyObject_GC_New(
        asynccallawaitableobject,
        &PyAsyncCallAwaitable_Type);
    if (awaitable == NULL) {
        return NULL;
    }

    Py_INCREF(it);
    awaitable->it = it;
    awaitable->wrapped_iter = wrapped_iter;

    _PyObject_GC_TRACK(awaitable);
    return (PyObject *) awaitable;
}

static PyObject *
asynccallawaitable_iternext(asynccallawaitableobject *obj)
{
    PyObject *result;
    PyObject *type, *value, *traceback;
    PyObject *stop_value = NULL;

    if (obj->it->it_sentinel == NULL) {
        return PyErr_Format(PyExc_TypeError, "'%.200s' object is already exhausted", obj->it);
    }

    result = (*Py_TYPE(obj->wrapped_iter)->tp_iternext)(obj->wrapped_iter);

    if (result != NULL) {
        return result;
    }

    if (PyErr_Occurred() == NULL) {
        PyErr_SetString(PyExc_AssertionError, "No exception set");
        return NULL;
    } else if (PyErr_ExceptionMatches(PyExc_StopIteration) == 0) {
        return result;
    }

    PyErr_Fetch(&type, &value, &traceback);
    PyErr_NormalizeException(&type, &value, &traceback);

    if (value != NULL) {
        stop_value = PyObject_GetAttrString(value, "value");
    }

    if (stop_value == NULL) {
        PyErr_SetString(PyExc_TypeError, "Coroutine iterator raised StopIteration without value");
        goto raise;
    }

    int ok = PyObject_RichCompareBool(obj->it->it_sentinel, stop_value, Py_EQ);
    Py_DECREF(stop_value);
    if (ok == 0) {
        PyErr_Restore(type, value, traceback);
        return NULL;
    }

    Py_CLEAR(obj->it->it_callable);
    Py_CLEAR(obj->it->it_sentinel);
    PyErr_SetNone(PyExc_StopAsyncIteration);

raise:
    Py_XDECREF(value);
    Py_XDECREF(type);
    Py_XDECREF(traceback);
    return NULL;
}

static PyObject *
anext_iternext(anextobject *obj)
{
    PyObject *result = PyIter_Next(obj->wrapped);
    if (result != NULL) {
        return result;
    }
    if (PyErr_ExceptionMatches(PyExc_StopAsyncIteration)) {
        _PyGen_SetStopIterationValue(obj->default_value);
    }
    return NULL;
}

static PyObject *
asynccalliter_reduce(asynccalliterobject *it, PyObject *Py_UNUSED(ignored))
{
    if (it->it_callable != NULL && it->it_sentinel != NULL)
        return Py_BuildValue("N(OO)",
        _PyEval_GetBuiltinId(&PyId_iter),  // TODO: not this
            it->it_callable, it->it_sentinel);
    else
        return Py_BuildValue(
            "N(())",
            _PyEval_GetBuiltinId(&PyId_iter)  // TODO: not this
        );
}

static PyMethodDef asynccalliter_methods[] = {
    {"__reduce__", (PyCFunction)asynccalliter_reduce, METH_NOARGS, reduce_doc},
    {NULL,              NULL}           /* sentinel */
};

static PyAsyncMethods async_iter_as_async = {
    PyObject_SelfIter,                          /* am_await */
    PyObject_SelfIter,                          /* am_aiter */
    (unaryfunc)asynccalliter_anext,                     /* am_anext */
    0,                                          /* am_send  */
};

PyTypeObject PyCallAiter_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "callable_async_iterator",                        /* tp_name */
    sizeof(asynccalliterobject),                     /* tp_basicsize */
    0,                                          /* tp_itemsize */
    /* methods */
    (destructor)asynccalliter_dealloc,               /* tp_dealloc */
    0,                                          /* tp_vectorcall_offset */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    &async_iter_as_async,                       /* tp_as_async */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,/* tp_flags */
    0,                                          /* tp_doc */
    (traverseproc)asynccalliter_traverse,            /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    PyObject_SelfIter,                          /* tp_iter */
    0,                                          /* tp_iternext */
    asynccalliter_methods,                           /* tp_methods */
};

static PyAsyncMethods async_awaitable_as_async = {
    PyObject_SelfIter,                          /* am_await */
    0,                                          /* am_aiter */
    0,                                          /* am_anext */
    0,                                          /* am_send  */
};

static PyMethodDef async_awaitable_methods[] = {
    // TODO...
    {"__reduce__", (PyCFunction)asynccalliter_reduce, METH_NOARGS, reduce_doc},
    {NULL,              NULL}           /* sentinel */
};

PyTypeObject PyAsyncCallAwaitable_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "callable_async_awaitable",                 /* tp_name */
    sizeof(asynccallawaitableobject),           /* tp_basicsize */
    0,                                          /* tp_itemsize */
    /* methods */
    (destructor)asynccallawaitable_dealloc,     /* tp_dealloc */
    0,                                          /* tp_vectorcall_offset */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    &async_awaitable_as_async,                  /* tp_as_async */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,    /* tp_flags */
    0,                                          /* tp_doc */
    (traverseproc)asynccallawaitable_traverse,  /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    PyObject_SelfIter,                          /* tp_iter */
    (unaryfunc)asynccallawaitable_iternext,     /* tp_iternext */
    async_awaitable_methods,                  /* tp_methods */
};

static PyAsyncMethods anext_as_async = {
    PyObject_SelfIter,                          /* am_await */
    0,                                          /* am_aiter */
    0,                                          /* am_anext */
    0,                                          /* am_send  */
};

PyTypeObject PyAnext_Type = {
    PyVarObject_HEAD_INIT(&PyType_Type, 0)
    "anext",                                    /* tp_name */
    sizeof(anextobject),                        /* tp_basicsize */
    0,                                          /* tp_itemsize */
    /* methods */
    (destructor)anext_dealloc,                  /* tp_dealloc */
    0,                                          /* tp_vectorcall_offset */
    0,                                          /* tp_getattr */
    0,                                          /* tp_setattr */
    &anext_as_async,                            /* tp_as_async */
    0,                                          /* tp_repr */
    0,                                          /* tp_as_number */
    0,                                          /* tp_as_sequence */
    0,                                          /* tp_as_mapping */
    0,                                          /* tp_hash */
    0,                                          /* tp_call */
    0,                                          /* tp_str */
    PyObject_GenericGetAttr,                    /* tp_getattro */
    0,                                          /* tp_setattro */
    0,                                          /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC,    /* tp_flags */
    0,                                          /* tp_doc */
    (traverseproc)anext_traverse,               /* tp_traverse */
    0,                                          /* tp_clear */
    0,                                          /* tp_richcompare */
    0,                                          /* tp_weaklistoffset */
    PyObject_SelfIter,                          /* tp_iter */
    (unaryfunc)anext_iternext,                  /* tp_iternext */
    0,                                          /* tp_methods */
};