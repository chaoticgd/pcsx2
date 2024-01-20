// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <QWidget>
#include "Models/DataInspectorModel.h"

#include "ui_SymbolTreeWidget.h"

struct SymbolFilters;

class SymbolTreeWidget : public QWidget
{
	Q_OBJECT

public:
	virtual ~SymbolTreeWidget();

	void setCPU(DebugInterface* cpu);

	void update();

protected:
	explicit SymbolTreeWidget(QWidget* parent = nullptr);
	
	void setupMenu();
	
	// Builds up the tree for when symbols are grouped by the module that
	// contains them, otherwise it just passes through to populateSections.
	std::vector<std::unique_ptr<DataInspectorNode>> populateModules(
		SymbolFilters& filters, const ccc::SymbolDatabase& database) const;
	
	// Builds up the tree for when symbols are grouped by the ELF section that
	// contains them, otherwise it just passes through to populateSourceFiles.
	std::vector<std::unique_ptr<DataInspectorNode>> populateSections(
		SymbolFilters& filters, const ccc::SymbolDatabase& database) const;

	// Builds up the tree for when symbols are grouped by the source file that
	// contains them, otherwise it just passes through to populateSymbols.
	std::vector<std::unique_ptr<DataInspectorNode>> populateSourceFiles(
		SymbolFilters& filters, const ccc::SymbolDatabase& database) const;

	// Generates a filtered list of symbols.
	virtual std::vector<std::unique_ptr<DataInspectorNode>> populateSymbols(
		const SymbolFilters& filters, const ccc::SymbolDatabase& database) const = 0;

	Ui::SymbolTreeWidget m_ui;

	DebugInterface* m_cpu = nullptr;
	DataInspectorModel* m_model = nullptr;
	
	QMenu* m_context_menu;
	QAction* m_group_by_module;
	QAction* m_group_by_section;
	QAction* m_group_by_source_file;
};

class FunctionTreeWidget : public SymbolTreeWidget
{
	Q_OBJECT
public:
	explicit FunctionTreeWidget(QWidget* parent = nullptr);
	virtual ~FunctionTreeWidget();

protected:
	std::vector<std::unique_ptr<DataInspectorNode>> populateSymbols(
		const SymbolFilters& filters, const ccc::SymbolDatabase& database) const override;
};

class GlobalVariableTreeWidget : public SymbolTreeWidget
{
	Q_OBJECT
public:
	explicit GlobalVariableTreeWidget(QWidget* parent = nullptr);
	virtual ~GlobalVariableTreeWidget();

protected:
	std::vector<std::unique_ptr<DataInspectorNode>> populateSymbols(
		const SymbolFilters& filters, const ccc::SymbolDatabase& database) const override;
};

struct SymbolFilters
{
	bool group_by_module = false;
	bool group_by_section = false;
	bool group_by_source_file = false;
	QString string;
	
	ccc::ModuleHandle module_handle;
	ccc::SectionHandle section;
	const ccc::SourceFile* source_file = nullptr;

	bool test(
		const ccc::Symbol& test_symbol,
		ccc::SourceFileHandle test_source_file,
		const ccc::SymbolDatabase& database,
		QString& name_out) const;
};
