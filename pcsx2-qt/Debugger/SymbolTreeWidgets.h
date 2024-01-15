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
	
	// Builds up the tree for when symbols are grouped by the ELF section that
	// contains them, otherwise it just passes through to populateSourceFiles.
	std::unique_ptr<DataInspectorNode> populateSections(
		bool group_by_section, bool group_by_source_file, const QString& filter, const ccc::SymbolDatabase& database) const;

	// Builds up the tree for when symbols are grouped by the source file that
	// contains them, otherwise it just passes through to populateSymbols.
	std::vector<std::unique_ptr<DataInspectorNode>> populateSourceFiles(
		u32 min_address, u32 max_address, bool group_by_source_file, const QString& filter, const ccc::SymbolDatabase& database) const;

	// Generates a filtered list of symbols.
	virtual std::vector<std::unique_ptr<DataInspectorNode>> populateSymbols(
		const SymbolFilters& filters, const ccc::SymbolDatabase& database) const = 0;

	Ui::SymbolTreeWidget m_ui;

	DebugInterface* m_cpu = nullptr;
	DataInspectorModel* m_model = nullptr;
	QMenu* m_context_menu = nullptr;
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
	// Filter by what source file the symbol belongs to.
	//   non-null     = Only symbols with a given source file.
	//   null         = Only symbol without a source file.
	//   std::nullopt = Don't filter by source file.
	std::optional<const ccc::SourceFile*> source_file;
	// Filter by address. This is used for grouping the symbols by the section
	// they belong to.
	u32 min_address = 0;
	u32 max_address = UINT32_MAX;
	// Filter by whether or not the name of the symbol contains a string.
	QString string;

	bool test(const ccc::Symbol& test_symbol, ccc::SourceFileHandle test_source_file, QString& name_out) const;
};
