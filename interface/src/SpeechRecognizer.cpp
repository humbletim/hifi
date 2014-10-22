//
//  SpeechRecognizer.cpp
//  interface/src
//
//  Created by David Rowe on 10/20/2014.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <QtGlobal>
#include <QDebug>

#include "SpeechRecognizer.h"

#ifdef Q_OS_WIN

SpeechRecognizer::SpeechRecognizer() :
    QObject(),
    _enabled(false),
    _commands(),
    _comInitialized(false),
    _speechRecognizer(NULL),
    _speechRecognizerContext(NULL),
    _speechRecognizerGrammar(NULL),
    _commandRecognizedEvent(NULL),
    _commandRecognizedNotifier(NULL) {

    HRESULT hr = ::CoInitialize(NULL);

    if (SUCCEEDED(hr)) {
        _comInitialized = true;
    }
}

SpeechRecognizer::~SpeechRecognizer() {

    if (_comInitialized) {
        ::CoUninitialize();
    }

    if (_speechRecognizerGrammar) {
        _speechRecognizerGrammar.Release();
    }
}

void SpeechRecognizer::handleCommandRecognized(const char* command) {
    emit commandRecognized(QString(command));
}

void SpeechRecognizer::setEnabled(bool enabled) {
    if (enabled == _enabled || !_comInitialized) {
        return;
    }

    _enabled = enabled;

    if (_enabled) {

        HRESULT hr = S_OK;

        // Create a shared recognizer.
        if (SUCCEEDED(hr)) {
            hr = _speechRecognizerContext.CoCreateInstance(CLSID_SpSharedRecoContext);
        }

        // Set up event notification mechanism.
        if (SUCCEEDED(hr)) {
            hr = _speechRecognizerContext->SetNotifyWin32Event();
        }
        if (SUCCEEDED(hr)) {
            _commandRecognizedEvent = _speechRecognizerContext->GetNotifyEventHandle();
            if (_commandRecognizedEvent == NULL) {
                hr = S_FALSE;
            }
        }
        if (SUCCEEDED(hr)) {
            _commandRecognizedNotifier = new QWinEventNotifier(_commandRecognizedEvent);
            connect(_commandRecognizedNotifier, SIGNAL(activated(HANDLE)), SLOT(notifyCommandRecognized(HANDLE)));
        }
        
        // Set which events to be notified of.
        if (SUCCEEDED(hr)) {
            hr = _speechRecognizerContext->SetInterest(SPFEI(SPEI_RECOGNITION), SPFEI(SPEI_RECOGNITION));
        }

        // Create grammar and load commands.
        if (SUCCEEDED(hr)) {
            hr = _speechRecognizerContext->CreateGrammar(NULL, &_speechRecognizerGrammar);
        }
        if (SUCCEEDED(hr)) {
            reloadCommands();
        }
        
        _enabled = SUCCEEDED(hr);

    } else {
        _speechRecognizerContext.Release();
    }

    emit enabledUpdated(_enabled);
}

void SpeechRecognizer::addCommand(const QString& command) {
    _commands.insert(command);
    reloadCommands();
}

void SpeechRecognizer::removeCommand(const QString& command) {
    _commands.remove(command);
    reloadCommands();
}

void SpeechRecognizer::reloadCommands() {
    if (!_enabled) {
        return;
    }

    HRESULT hr = S_OK;

    if (SUCCEEDED(hr)) {
        hr = _speechRecognizerContext->Pause(NULL);
    }

    if (SUCCEEDED(hr)) {
        WORD langId = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);
        hr = _speechRecognizerGrammar->ResetGrammar(langId);
    }

    DWORD ruleID = 0;
    SPSTATEHANDLE initialState;
    for (QSet<QString>::const_iterator iter = _commands.constBegin(); iter != _commands.constEnd(); iter++) {
        ruleID += 1;

        if (SUCCEEDED(hr)) {
            hr = _speechRecognizerGrammar->GetRule(NULL, ruleID, SPRAF_TopLevel | SPRAF_Active | SPRAF_Dynamic, TRUE, &initialState);
        }

        if (SUCCEEDED(hr)) {
            const std::wstring command = (*iter).toStdWString();  // DJRTODO: Better, shorter name
            hr = _speechRecognizerGrammar->AddWordTransition(initialState, NULL, command.c_str(), L" ", SPWT_LEXICAL, 1.0, NULL);
        }
    }

    if (SUCCEEDED(hr)) {
        hr = _speechRecognizerGrammar->Commit(0);
    }

    if (SUCCEEDED(hr)) {
        hr = _speechRecognizerContext->Resume(NULL);
    }

    if (SUCCEEDED(hr)) {
        hr = _speechRecognizerGrammar->SetRuleState(NULL, NULL, SPRS_ACTIVE);
    }

    if (FAILED(hr)) {
        qDebug() << "ERROR: Didn't successfully reload commands";
    }
}

void SpeechRecognizer::notifyCommandRecognized(HANDLE handle) {

    SPEVENT eventItem;
    memset(&eventItem, 0, sizeof(SPEVENT));
    HRESULT hr = _speechRecognizerContext->GetEvents(1, &eventItem, NULL);

    if (SUCCEEDED(hr)) {
        if (eventItem.eEventId == SPEI_RECOGNITION && eventItem.elParamType == SPET_LPARAM_IS_OBJECT) {
            ISpRecoResult* recognitionResult = reinterpret_cast<ISpRecoResult*>(eventItem.lParam);
            wchar_t* pText;

            hr = recognitionResult->GetText(SP_GETWHOLEPHRASE, SP_GETWHOLEPHRASE, FALSE, &pText, NULL);

            if (SUCCEEDED(hr)) {
                QString text = QString::fromWCharArray(pText);
                handleCommandRecognized(text.toStdString().c_str());
                ::CoTaskMemFree(pText);
            }

            recognitionResult->Release();
        }
    }
}

#endif // Q_OS_WIN