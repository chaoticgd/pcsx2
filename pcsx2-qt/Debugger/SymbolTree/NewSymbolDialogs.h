// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QDialog>

#include "DebugTools/DebugInterface.h"
#include "ui_NewSymbolDialog.h"

// Base class for symbol creation dialogs.
class NewSymbolDialog : public QDialog
{
	Q_OBJECT

public:
	virtual void createSymbol() = 0;

protected:
	explicit NewSymbolDialog(u32 flags, DebugInterface& cpu, QWidget* parent = nullptr);

	enum Flags
	{
		GLOBAL_STORAGE = 1 << 0,
		REGISTER_STORAGE = 1 << 1,
		STACK_STORAGE = 1 << 2,
		SIZE_FIELD = 1 << 3,
		EXISTING_FUNCTIONS_FIELD = 1 << 4,
		TYPE_FIELD = 1 << 5,
		FUNCTION_FIELD = 1 << 6
	};

	void setupRegisterField();
	void setupFunctionField();
	void onStorageTabChanged(int index);
	u32 storageType() const;

	DebugInterface& m_cpu;
	Ui::NewSymbolDialog m_ui;
	
	std::vector<ccc::FunctionHandle> m_functions;
};

class NewFunctionDialog : public NewSymbolDialog
{
	Q_OBJECT

public:
	NewFunctionDialog(DebugInterface& cpu, QWidget* parent = nullptr);

protected:
	void createSymbol() override;
};

class NewGlobalVariableDialog : public NewSymbolDialog
{
	Q_OBJECT

public:
	NewGlobalVariableDialog(DebugInterface& cpu, QWidget* parent = nullptr);

protected:
	void createSymbol() override;
};

class NewLocalVariableDialog : public NewSymbolDialog
{
	Q_OBJECT

public:
	NewLocalVariableDialog(DebugInterface& cpu, QWidget* parent = nullptr);

protected:
	void createSymbol() override;
};

class NewParameterVariableDialog : public NewSymbolDialog
{
	Q_OBJECT

public:
	NewParameterVariableDialog(DebugInterface& cpu, QWidget* parent = nullptr);

protected:
	void createSymbol() override;
};
