/*
 * Copyright (C) 2012-2013, 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "ProfilerDatabase.h"

#include "CodeBlock.h"
#include "JSONObject.h"
#include "ObjectConstructor.h"
#include "JSCInlines.h"
#include <wtf/CurrentTime.h>

namespace JSC { namespace Profiler {

static std::atomic<int> databaseCounter;

static StaticLock registrationLock;
static std::atomic<int> didRegisterAtExit;
static Database* firstDatabase;

Database::Database(VM& vm)
    : m_databaseID(++databaseCounter)
    , m_vm(vm)
    , m_shouldSaveAtExit(false)
    , m_nextRegisteredDatabase(0)
{
}

Database::~Database()
{
    if (m_shouldSaveAtExit) {
        removeDatabaseFromAtExit();
        performAtExitSave();
    }
}

Bytecodes* Database::ensureBytecodesFor(CodeBlock* codeBlock)
{
    LockHolder locker(m_lock);
    return ensureBytecodesFor(locker, codeBlock);
}

Bytecodes* Database::ensureBytecodesFor(const LockHolder&, CodeBlock* codeBlock)
{
    codeBlock = codeBlock->baselineVersion();
    
    HashMap<CodeBlock*, Bytecodes*>::iterator iter = m_bytecodesMap.find(codeBlock);
    if (iter != m_bytecodesMap.end())
        return iter->value;
    
    m_bytecodes.append(Bytecodes(m_bytecodes.size(), codeBlock));
    Bytecodes* result = &m_bytecodes.last();
    
    m_bytecodesMap.add(codeBlock, result);
    
    return result;
}

void Database::notifyDestruction(CodeBlock* codeBlock)
{
    LockHolder locker(m_lock);
    
    m_bytecodesMap.remove(codeBlock);
    m_compilationMap.remove(codeBlock);
}

void Database::addCompilation(CodeBlock* codeBlock, PassRefPtr<Compilation> passedCompilation)
{
    LockHolder locker(m_lock);
    ASSERT(!isCompilationThread());

    RefPtr<Compilation> compilation = passedCompilation;
    
    m_compilations.append(compilation);
    m_compilationMap.set(codeBlock, compilation);
}

JSValue Database::toJS(ExecState* exec) const
{
    JSObject* result = constructEmptyObject(exec);
    
    JSArray* bytecodes = constructEmptyArray(exec, 0);
    for (unsigned i = 0; i < m_bytecodes.size(); ++i)
        bytecodes->putDirectIndex(exec, i, m_bytecodes[i].toJS(exec));
    result->putDirect(exec->vm(), exec->propertyNames().bytecodes, bytecodes);
    
    JSArray* compilations = constructEmptyArray(exec, 0);
    for (unsigned i = 0; i < m_compilations.size(); ++i)
        compilations->putDirectIndex(exec, i, m_compilations[i]->toJS(exec));
    result->putDirect(exec->vm(), exec->propertyNames().compilations, compilations);
    
    JSArray* events = constructEmptyArray(exec, 0);
    for (unsigned i = 0; i < m_events.size(); ++i)
        events->putDirectIndex(exec, i, m_events[i].toJS(exec));
    result->putDirect(exec->vm(), exec->propertyNames().events, events);
    
    return result;
}

String Database::toJSON() const
{
    JSGlobalObject* globalObject = JSGlobalObject::create(
        m_vm, JSGlobalObject::createStructure(m_vm, jsNull()));
    
    return JSONStringify(globalObject->globalExec(), toJS(globalObject->globalExec()), 0);
}

bool Database::save(const char* filename) const
{
    auto out = FilePrintStream::open(filename, "w");
    if (!out)
        return false;
    
    out->print(toJSON());
    return true;
}

void Database::registerToSaveAtExit(const char* filename)
{
    m_atExitSaveFilename = filename;
    
    if (m_shouldSaveAtExit)
        return;
    
    addDatabaseToAtExit();
    m_shouldSaveAtExit = true;
}

void Database::logEvent(CodeBlock* codeBlock, const char* summary, const CString& detail)
{
    LockHolder locker(m_lock);
    
    Bytecodes* bytecodes = ensureBytecodesFor(locker, codeBlock);
    Compilation* compilation = m_compilationMap.get(codeBlock);
    m_events.append(Event(currentTime(), bytecodes, compilation, summary, detail));
}

void Database::addDatabaseToAtExit()
{
    if (++didRegisterAtExit == 1)
        atexit(atExitCallback);
    
    LockHolder holder(registrationLock);
    m_nextRegisteredDatabase = firstDatabase;
    firstDatabase = this;
}

void Database::removeDatabaseFromAtExit()
{
    LockHolder holder(registrationLock);
    for (Database** current = &firstDatabase; *current; current = &(*current)->m_nextRegisteredDatabase) {
        if (*current != this)
            continue;
        *current = m_nextRegisteredDatabase;
        m_nextRegisteredDatabase = 0;
        m_shouldSaveAtExit = false;
        break;
    }
}

void Database::performAtExitSave() const
{
    save(m_atExitSaveFilename.data());
}

Database* Database::removeFirstAtExitDatabase()
{
    LockHolder holder(registrationLock);
    Database* result = firstDatabase;
    if (result) {
        firstDatabase = result->m_nextRegisteredDatabase;
        result->m_nextRegisteredDatabase = 0;
        result->m_shouldSaveAtExit = false;
    }
    return result;
}

void Database::atExitCallback()
{
    while (Database* database = removeFirstAtExitDatabase())
        database->performAtExitSave();
}

} } // namespace JSC::Profiler

