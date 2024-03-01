// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QtWidgets/QDialog>

#include "DebugTools/DebugInterface.h"
#include "ui_NewSymbolDialog.h"

// Base class for symbol creation dialogs.
class NewSymbolDialog : public QDialog
{
	Q_OBJECT

public:
	// Used to apply default settings.
	void setAddress(u32 address);
	void setCustomSize(u32 size);

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

	// Used for setting up row visibility. Keep in sync with the .ui file!
	enum Row
	{
		NAME,
		ADDRESS,
		REGISTER,
		STACK_POINTER_OFFSET,
		SIZE,
		EXISTING_FUNCTIONS,
		TYPE,
		FUNCTION
	};

	virtual void createSymbol() = 0;

	void setupRegisterField();
	void setupSizeField();
	void setupFunctionField();

	enum FunctionSizeType
	{
		FILL_EXISTING_FUNCTION,
		FILL_EMPTY_SPACE,
		CUSTOM_SIZE
	};

	FunctionSizeType functionSizeType() const;
	void updateSizeField();
	std::optional<u32> fillExistingFunctionSize(u32 address, const ccc::SymbolDatabase& database);
	std::optional<u32> fillEmptySpaceSize(u32 address, const ccc::SymbolDatabase& database);

	u32 storageType() const;
	void onStorageTabChanged(int index);

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
