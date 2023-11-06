/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QtWidgets/QMainWindow>

#include "Elfheader.h"
#include "Models/DataInspectorModel.h"
#include "ui_DataInspectorWindow.h"

class DataInspectorWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit DataInspectorWindow(QWidget* parent = nullptr);

	void resetGlobals();

protected:
	void loadMdebugSymbolTableAsync(
		std::function<void(ccc::HighSymbolTable& symbolTable, std::vector<std::pair<ELF_SHR, std::string>>& elfSections)> successCallback,
		std::function<void(QString errorMessage, const char* sourceFile, int sourceLine)> failureCallback);
	
	
	void createGUI();
	std::unique_ptr<DataInspectorNode> populateGlobalSections(
		bool groupBySection, bool groupByTranslationUnit, const QString& filter);
	std::vector<std::unique_ptr<DataInspectorNode>> populateGlobalTranslationUnits(
		u32 minAddress, u32 maxAddress, bool groupByTranlationUnit, const QString& filter);
	std::vector<std::unique_ptr<DataInspectorNode>> populateGlobalVariables(
		const ccc::ast::SourceFile& sourceFile, u32 minAddress, u32 maxAddress, const QString& filter);

	ccc::HighSymbolTable m_symbolTable;
	std::vector<std::pair<ELF_SHR, std::string>> m_elfSections;
	DataInspectorModel* m_globalModel;
	Ui::DataInspectorWindow m_ui;

	std::map<std::string, s32> m_typeNameToDeduplicatedTypeIndex;
};
