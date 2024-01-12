// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#ifndef GLOBALVARIABLESWIDGET_H
#define GLOBALVARIABLESWIDGET_H

#include <QWidget>
#include "Models/DataInspectorModel.h"

#include "ui_GlobalVariablesWidget.h"

class GlobalVariablesWidget : public QWidget
{
	Q_OBJECT

public:
	explicit GlobalVariablesWidget(DebugInterface& cpu, QWidget* parent = nullptr);
	~GlobalVariablesWidget();

	void update();

	std::unique_ptr<DataInspectorNode> populateGlobalSections(
		bool group_by_section, bool group_by_source_file, const QString& filter, const ccc::SymbolDatabase& database);
	std::vector<std::unique_ptr<DataInspectorNode>> populateGlobalSourceFiles(
		u32 min_address, u32 max_address, bool group_by_source_file, const QString& filter, const ccc::SymbolDatabase& database);
	std::vector<std::unique_ptr<DataInspectorNode>> populateGlobalVariables(
		const ccc::SourceFile& source_file, u32 min_address, u32 max_address, const QString& filter, const ccc::SymbolDatabase& database);

private:
	Ui::GlobalVariablesWidget m_ui;

	DebugInterface& m_cpu;
	DataInspectorModel* m_model = nullptr;
};

#endif // GLOBALVARIABLESWIDGET_H
