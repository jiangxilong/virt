#ifndef TABLE_SERIALIZE
#define TABLE_SERIALIZE

#include <QtCore>
#include <QtWidgets>

inline QStringList serializeToStringList(const QTableWidget* table) {
	QStringList strings;
	const auto nRow = table->rowCount();
	const auto nColumn = table->columnCount();
	const auto nItems = nRow * nColumn;

	if (nColumn > 0
		&& nRow > 0
		&& nItems / nColumn == nRow
		) {
		strings.reserve(nItems);
		for (int row = 0; row < nRow; ++row)
		for (int column = 0; column < nColumn; ++column) {
			QString value;
			auto item = table->item(row, column);
			if (item) {
				value = item->text();
			}
			strings.push_back(value);
		}
	}

	return strings;
}

inline QTableWidgetItem* item(QTableWidget* table, int row, int column) {
	auto item = table->item(row, column);
	if (!item) {
		table->setItem(row, column, item = new QTableWidgetItem());
	}
	return item;
}

inline QTableWidgetItem* setItemText(
	QTableWidget* table, 
	int row, int column,
	const QString& text
) {
	const auto item = ::item(table, row, column);
	item->setText(text);
	return item;
}

inline void deserialize(QTableWidget* table, const QStringList& strings) {
	const auto nColumn = table->columnCount();
	if (nColumn > 0) {
		const auto nItem = strings.size();
		const auto nRow = nItem / nColumn + !!(nItem % nColumn);

		table->setRowCount(nRow);
		for (int i = 0; i < nItem; ++i) {
			setItemText(table, i / nColumn, i % nColumn, strings[i]);
		}
	}
}

inline void serialize(QSettings& settings, const QTableWidget* table) {
	const auto name = table->objectName();
	settings.setValue(name, serializeToStringList(table));
	settings.setValue(name + "CurrentRow", table->currentRow());
}

inline void deserialize(QTableWidget* table, const QSettings& settings) {
	const QSignalBlocker signalBlocker(table);
	const auto name = table->objectName();
	deserialize(table, settings.value(name).toStringList());
	table->selectRow(settings.value(name + "CurrentRow").toInt());
}

#endif