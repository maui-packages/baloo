/*
   Copyright (c) 2012-2013 Vishesh Handa <me@vhanda.in>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QStandardPaths>

#include <KAboutData>
#include <KLocalizedString>

#include "file.h"
#include "filefetchjob.h"
#include "searchstore.h" // for deserialize

#include <KFileMetaData/PropertyInfo>

#include "src/xapian/xapiandatabase.h"
#include "src/xapian/xapiandocument.h"

QString colorString(const QString& input, int color)
{
    QString colorStart = QString::fromLatin1("\033[0;%1m").arg(color);
    QLatin1String colorEnd("\033[0;0m");

    return colorStart + input + colorEnd;
}

int main(int argc, char* argv[])
{
    KAboutData aboutData(QLatin1String("balooshow"),
                         i18n("Baloo Show"),
                         PROJECT_VERSION,
                         i18n("The Baloo data Viewer - A debugging tool"),
                         KAboutLicense::GPL,
                         i18n("(c) 2012, Vishesh Handa"));
    aboutData.addAuthor(i18n("Vishesh Handa"), i18n("Maintainer"), QLatin1String("me@vhanda.in"));

    KAboutData::setApplicationData(aboutData);
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.addPositionalArgument(QLatin1String("files"), QLatin1String("The file urls"));
    parser.addOption(QCommandLineOption(QStringList() << QStringLiteral("x") << QStringLiteral("xapian"),
                                        QStringLiteral("Print internal xapian info")));
    parser.addHelpOption();
    parser.process(app);

    QStringList args = parser.positionalArguments();

    if (args.isEmpty()) {
        parser.showHelp(1);
    }

    //
    // The Resource Uri
    //
    QStringList urls;
    Q_FOREACH (const QString& arg, args) {
        const QString url = QFileInfo(arg).absoluteFilePath();
        if (QFile::exists(url)) {
            urls.append(url);
        } else {
            urls.append(QLatin1String("file:") + arg);
        }
    }

    QTextStream stream(stdout);
    Baloo::FileFetchJob* job;
    QString text;

    Q_FOREACH (const QString& url, urls) {
        Baloo::File ifile;
        if (url.startsWith(QLatin1String("file:"))) {
            ifile.setId(url.toUtf8());
        }
        else {
            ifile.setUrl(url);
        }
        job = new Baloo::FileFetchJob(ifile);
        job->exec();

        Baloo::File file = job->file();
        int fid = Baloo::deserialize("file", file.id());

        if (fid && !file.url().isEmpty()) {
            text = colorString(QString::number(fid), 31);
            text += QLatin1String(" ");
            text += colorString(file.url(), 32);
            stream << text << endl;
        }
        else {
            stream << "No index information found" << endl;
        }

        KFileMetaData::PropertyMap propMap = file.properties();
        KFileMetaData::PropertyMap::const_iterator it = propMap.constBegin();
        for (; it != propMap.constEnd(); ++it) {
            KFileMetaData::PropertyInfo pi(it.key());
            stream << "\t" << pi.displayName() << ": " << it.value().toString() << endl;
        }

        if (file.rating())
            stream << "\t" << "Rating: " << file.rating() << endl;

        if (!file.tags().isEmpty())
            stream << "\t" << "Tags: " << file.tags().join(QLatin1String(", ")) << endl;

        if (!file.userComment().isEmpty())
            stream << "\t" << "User Comment: " << file.userComment() << endl;

        if (parser.isSet(QStringLiteral("xapian"))) {
            const QString path = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                                + QLatin1String("/baloo/file");

            Baloo::XapianDatabase db(path);
            Baloo::XapianDocument xapDoc = db.document(fid);
            Xapian::Document doc = xapDoc.doc();

            QStringList prefixedWords;
            QStringList normalWords;
            QHash<int, QStringList> propertyWords;

            for (auto it = doc.termlist_begin(); it != doc.termlist_end(); ++it) {
                std::string str = *it;
                QString word = QString::fromUtf8(str.c_str(), str.length());

                if (word[0].isUpper()) {
                    if (word[0] == QLatin1Char('X')) {
                        int posOfNonNumeric = 1;
                        while (word[posOfNonNumeric].isNumber() && posOfNonNumeric < 3) {
                            posOfNonNumeric++;
                        }

                        int propNum = word.mid(1, posOfNonNumeric-1).toInt();
                        QString value = word.mid(posOfNonNumeric);

                        propertyWords[propNum].append(value);
                    }
                    else {
                        prefixedWords << word;
                    }
                } else {
                    normalWords << word;
                }
            }

            stream << "\nXapian Internal Info\n" << endl;
            stream << "Words: " << normalWords.join(" ") << endl << endl;
            stream << "Prefixed Words: " << prefixedWords.join(" ") << endl << endl;

            for (auto it = propertyWords.constBegin(); it != propertyWords.constEnd(); it++) {
                auto prop = static_cast<KFileMetaData::Property::Property>(it.key());
                KFileMetaData::PropertyInfo pi(prop);

                stream << pi.name() << ": " << it.value().join(" ") << endl;
            }
        }
    }

    return 0;
}
