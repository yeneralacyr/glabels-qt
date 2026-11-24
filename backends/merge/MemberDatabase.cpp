//  MemberDatabase.cpp
//
//  Copyright (C) 2026  gLabels-qt contributors
//
//  This file is part of gLabels-qt.
//
//  gLabels-qt is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  gLabels-qt is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with gLabels-qt.  If not, see <http://www.gnu.org/licenses/>.
//

#include "MemberDatabase.hpp"

#include <QCollator>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QVariant>

#include <algorithm>


namespace
{
        const int SCHEMA_VERSION = 1;

        const char* DB_ENV_OVERRIDE = "GLABELS_UYELER_DB";


        /// Column name for a standard field key (prefixed to avoid SQL keywords such as NOT)
        QString columnFor( const QString& key )
        {
                return QStringLiteral( "f_" ) + key.toLower();
        }


        /// Qt binds a null QString as SQL NULL; columns are NOT NULL, so always bind real text
        QVariant textValue( const QString& s )
        {
                QString t = s.trimmed();
                return QVariant( t.isEmpty() ? QStringLiteral( "" ) : t );
        }


        QString sqlErrorText( const QSqlError& e )
        {
                QString text = e.text().trimmed();
                return text.isEmpty() ? QStringLiteral( "bilinmeyen veritabanı hatası" ) : text;
        }

}


namespace glabels::merge
{

        ///
        /// Constructor
        ///
        MemberDatabase::MemberDatabase()
                : mConnectionName( QStringLiteral( "glabels-uyeler" ) )
        {
                QString overridePath = qEnvironmentVariable( DB_ENV_OVERRIDE );
                if ( !overridePath.isEmpty() )
                {
                        mFilePath = QFileInfo( overridePath ).absoluteFilePath();
                }
                else
                {
                        QString base = QStandardPaths::writableLocation( QStandardPaths::GenericDataLocation );
                        mFilePath = QDir( base ).filePath( QStringLiteral( "glabels-uyeler/uyeler.sqlite" ) );
                }
        }


        ///
        /// Destructor
        ///
        MemberDatabase::~MemberDatabase()
        {
                if ( QSqlDatabase::contains( mConnectionName ) )
                {
                        {
                                QSqlDatabase db = QSqlDatabase::database( mConnectionName, false );
                                db.close();
                        }
                        QSqlDatabase::removeDatabase( mConnectionName );
                }
        }


        ///
        /// Singleton
        ///
        MemberDatabase* MemberDatabase::instance()
        {
                static MemberDatabase* singleton = new MemberDatabase();
                return singleton;
        }


        ///
        /// Standard fields (order = display order)
        ///
        const QList<MemberField>& MemberDatabase::standardFields()
        {
                static const QList<MemberField> fields = {
                        { "FIRMA",         "Firma Ünvanı",    false },
                        { "AD",            "Yetkili Adı",     false },
                        { "SOYAD",         "Yetkili Soyadı",  false },
                        { "SICIL_NO",      "Sicil No",        false },
                        { "MESLEK_GRUBU",  "Meslek Grubu",    false },
                        { "VERGI_DAIRESI", "Vergi Dairesi",   false },
                        { "VERGI_NO",      "Vergi No",        false },
                        { "ADRES",         "Adres",           true  },
                        { "ILCE",          "İlçe",            false },
                        { "IL",            "İl",              false },
                        { "TELEFON",       "Telefon",         false },
                        { "EPOSTA",        "E-posta",         false },
                        { "NOT",           "Not",             true  },
                };
                return fields;
        }


        QString MemberDatabase::idKey()
        {
                return QStringLiteral( "UYE_ID" );
        }


        QString MemberDatabase::activeKey()
        {
                return QStringLiteral( "AKTIF" );
        }


        ///
        /// Custom keys: upper-case ASCII, digits and underscore, not clashing with built-ins
        ///
        bool MemberDatabase::isValidCustomKey( const QString& key )
        {
                static const QRegularExpression re( QStringLiteral( "^[A-Z][A-Z0-9_]{0,39}$" ) );
                if ( !re.match( key ).hasMatch() )
                {
                        return false;
                }
                if ( key == idKey() || key == activeKey() )
                {
                        return false;
                }
                for ( const auto& f : standardFields() )
                {
                        if ( f.key == key )
                        {
                                return false;
                        }
                }
                return true;
        }


        ///
        /// Suggest a template key from a Turkish label, e.g. "Oda Kayıt Tarihi" -> "ODA_KAYIT_TARIHI"
        ///
        QString MemberDatabase::suggestKey( const QString& label )
        {
                QString upper = QLocale( QLocale::Turkish ).toUpper( label.trimmed() );

                static const QMap<QChar,QChar> map = {
                        { QChar( 0x00C7 ), 'C' },  // Ç
                        { QChar( 0x011E ), 'G' },  // Ğ
                        { QChar( 0x0130 ), 'I' },  // İ
                        { QChar( 0x00D6 ), 'O' },  // Ö
                        { QChar( 0x015E ), 'S' },  // Ş
                        { QChar( 0x00DC ), 'U' },  // Ü
                        { QChar( 0x00C2 ), 'A' },  // Â
                        { QChar( 0x00CE ), 'I' },  // Î
                        { QChar( 0x00DB ), 'U' },  // Û
                };

                QString key;
                bool lastUnderscore = false;
                for ( QChar c : upper )
                {
                        auto it = map.find( c );
                        if ( it != map.end() )
                        {
                                c = it.value();
                        }

                        if ( (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') )
                        {
                                key.append( c );
                                lastUnderscore = false;
                        }
                        else if ( !key.isEmpty() && !lastUnderscore )
                        {
                                key.append( '_' );
                                lastUnderscore = true;
                        }
                }
                while ( key.endsWith( '_' ) )
                {
                        key.chop( 1 );
                }
                if ( !key.isEmpty() && key[0].isDigit() )
                {
                        key.prepend( QStringLiteral( "ALAN_" ) );
                }
                return key.left( 40 );
        }


        ///
        /// Record error and propagate
        ///
        bool MemberDatabase::fail( const QString& message, QString* error )
        {
                mLastError = message;
                qWarning( "MemberDatabase: %s", qUtf8Printable( message ) );
                if ( error )
                {
                        *error = message;
                }
                return false;
        }


        bool MemberDatabase::isOpen() const
        {
                return mOpen;
        }


        QString MemberDatabase::filePath() const
        {
                return mFilePath;
        }


        QString MemberDatabase::lastError() const
        {
                return mLastError;
        }


        ///
        /// Open (and create if needed) the database file
        ///
        bool MemberDatabase::ensureOpen( QString* error )
        {
                if ( mOpen )
                {
                        return true;
                }

                if ( !QSqlDatabase::isDriverAvailable( QStringLiteral( "QSQLITE" ) ) )
                {
                        return fail( QStringLiteral( "SQLite sürücüsü (QSQLITE) bulunamadı. Kurulum eksik olabilir." ), error );
                }

                QDir dir = QFileInfo( mFilePath ).absoluteDir();
                if ( !dir.exists() && !dir.mkpath( QStringLiteral( "." ) ) )
                {
                        return fail( QStringLiteral( "Veritabanı klasörü oluşturulamadı: %1" ).arg( dir.absolutePath() ), error );
                }

                QSqlDatabase db = QSqlDatabase::contains( mConnectionName )
                        ? QSqlDatabase::database( mConnectionName, false )
                        : QSqlDatabase::addDatabase( QStringLiteral( "QSQLITE" ), mConnectionName );
                db.setDatabaseName( mFilePath );

                if ( !db.open() )
                {
                        return fail( QStringLiteral( "Veritabanı açılamadı (%1): %2" )
                                     .arg( mFilePath, sqlErrorText( db.lastError() ) ), error );
                }

                QSqlQuery q( db );
                if ( !q.exec( QStringLiteral( "PRAGMA foreign_keys = ON" ) ) )
                {
                        db.close();
                        return fail( QStringLiteral( "Veritabanı ayarı yapılamadı: %1" ).arg( sqlErrorText( q.lastError() ) ), error );
                }

                if ( !createSchema( error ) )
                {
                        db.close();
                        return false;
                }

                mOpen = true;
                return true;
        }


        ///
        /// Create / migrate schema
        ///
        bool MemberDatabase::createSchema( QString* error )
        {
                QSqlDatabase db = QSqlDatabase::database( mConnectionName, false );
                QSqlQuery q( db );

                if ( !q.exec( QStringLiteral( "PRAGMA user_version" ) ) || !q.next() )
                {
                        return fail( QStringLiteral( "Şema sürümü okunamadı: %1" ).arg( sqlErrorText( q.lastError() ) ), error );
                }
                int version = q.value( 0 ).toInt();
                q.finish();

                if ( version > SCHEMA_VERSION )
                {
                        return fail( QStringLiteral( "Veritabanı bu programdan daha yeni bir sürümle oluşturulmuş (sürüm %1). "
                                                     "Lütfen programı güncelleyin." ).arg( version ), error );
                }
                if ( version == SCHEMA_VERSION )
                {
                        return true;
                }

                QStringList columns;
                for ( const auto& f : standardFields() )
                {
                        columns << QStringLiteral( "%1 TEXT NOT NULL DEFAULT ''" ).arg( columnFor( f.key ) );
                }

                QStringList statements = {
                        QStringLiteral( "CREATE TABLE IF NOT EXISTS members ("
                                        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                        " active INTEGER NOT NULL DEFAULT 1,"
                                        " %1,"
                                        " created_at TEXT NOT NULL DEFAULT (datetime('now')),"
                                        " updated_at TEXT NOT NULL DEFAULT (datetime('now')) )" ).arg( columns.join( ", " ) ),
                        QStringLiteral( "CREATE TABLE IF NOT EXISTS custom_fields ("
                                        " field_key TEXT PRIMARY KEY,"
                                        " label TEXT NOT NULL,"
                                        " sort_order INTEGER NOT NULL )" ),
                        QStringLiteral( "CREATE TABLE IF NOT EXISTS member_values ("
                                        " member_id INTEGER NOT NULL REFERENCES members(id) ON DELETE CASCADE,"
                                        " field_key TEXT NOT NULL REFERENCES custom_fields(field_key) ON DELETE CASCADE,"
                                        " value TEXT NOT NULL,"
                                        " PRIMARY KEY (member_id, field_key) )" ),
                        QStringLiteral( "CREATE INDEX IF NOT EXISTS idx_member_values_field ON member_values(field_key)" ),
                        QStringLiteral( "PRAGMA user_version = %1" ).arg( SCHEMA_VERSION ),
                };

                if ( !db.transaction() )
                {
                        return fail( QStringLiteral( "İşlem başlatılamadı: %1" ).arg( sqlErrorText( db.lastError() ) ), error );
                }
                for ( const auto& sql : statements )
                {
                        if ( !q.exec( sql ) )
                        {
                                QString msg = sqlErrorText( q.lastError() );
                                db.rollback();
                                return fail( QStringLiteral( "Veritabanı şeması oluşturulamadı: %1" ).arg( msg ), error );
                        }
                }
                if ( !db.commit() )
                {
                        QString msg = sqlErrorText( db.lastError() );
                        db.rollback();
                        return fail( QStringLiteral( "Veritabanı şeması kaydedilemedi: %1" ).arg( msg ), error );
                }
                return true;
        }


        ///
        /// Consistent copy of the database to another file
        ///
        bool MemberDatabase::backupTo( const QString& destPath, QString* error )
        {
                if ( !ensureOpen( error ) )
                {
                        return false;
                }

                QString absDest = QFileInfo( destPath ).absoluteFilePath();
                if ( absDest == QFileInfo( mFilePath ).absoluteFilePath() )
                {
                        return fail( QStringLiteral( "Yedek, veritabanının kendisinin üzerine yazılamaz." ), error );
                }
                if ( QFile::exists( absDest ) && !QFile::remove( absDest ) )
                {
                        return fail( QStringLiteral( "Var olan dosya silinemedi: %1" ).arg( absDest ), error );
                }

                QSqlQuery q( QSqlDatabase::database( mConnectionName, false ) );
                q.prepare( QStringLiteral( "VACUUM INTO ?" ) );
                q.addBindValue( absDest );
                if ( !q.exec() )
                {
                        return fail( QStringLiteral( "Yedek alınamadı: %1" ).arg( sqlErrorText( q.lastError() ) ), error );
                }
                return true;
        }


        ///
        /// Custom field list
        ///
        bool MemberDatabase::customFields( QList<MemberField>& fields, QString* error )
        {
                fields.clear();
                if ( !ensureOpen( error ) )
                {
                        return false;
                }

                QSqlQuery q( QSqlDatabase::database( mConnectionName, false ) );
                if ( !q.exec( QStringLiteral( "SELECT field_key, label FROM custom_fields ORDER BY sort_order, field_key" ) ) )
                {
                        return fail( QStringLiteral( "Ek alanlar okunamadı: %1" ).arg( sqlErrorText( q.lastError() ) ), error );
                }
                while ( q.next() )
                {
                        fields.append( { q.value( 0 ).toString(), q.value( 1 ).toString(), false } );
                }
                return true;
        }


        bool MemberDatabase::allFields( QList<MemberField>& fields, QString* error )
        {
                QList<MemberField> custom;
                if ( !customFields( custom, error ) )
                {
                        fields.clear();
                        return false;
                }
                fields = standardFields();
                fields.append( custom );
                return true;
        }


        bool MemberDatabase::addCustomField( const MemberField& field, QString* error )
        {
                if ( !ensureOpen( error ) )
                {
                        return false;
                }
                if ( field.label.trimmed().isEmpty() )
                {
                        return fail( QStringLiteral( "Alan adı boş olamaz." ), error );
                }
                if ( !isValidCustomKey( field.key ) )
                {
                        return fail( QStringLiteral( "Geçersiz şablon anahtarı \"%1\". Büyük harf (A-Z), rakam ve _ kullanın; "
                                                     "harfle başlamalı ve hazır alanlarla aynı olmamalı." ).arg( field.key ), error );
                }

                QSqlQuery q( QSqlDatabase::database( mConnectionName, false ) );
                q.prepare( QStringLiteral( "INSERT INTO custom_fields (field_key, label, sort_order) "
                                           "VALUES (?, ?, (SELECT COALESCE(MAX(sort_order), 0) + 1 FROM custom_fields))" ) );
                q.addBindValue( field.key );
                q.addBindValue( textValue( field.label ) );
                if ( !q.exec() )
                {
                        return fail( QStringLiteral( "Ek alan eklenemedi (anahtar zaten kullanılıyor olabilir): %1" )
                                     .arg( sqlErrorText( q.lastError() ) ), error );
                }

                emit changed();
                return true;
        }


        bool MemberDatabase::removeCustomField( const QString& key, QString* error )
        {
                if ( !ensureOpen( error ) )
                {
                        return false;
                }

                QSqlQuery q( QSqlDatabase::database( mConnectionName, false ) );
                q.prepare( QStringLiteral( "DELETE FROM custom_fields WHERE field_key = ?" ) );
                q.addBindValue( key );
                if ( !q.exec() )
                {
                        return fail( QStringLiteral( "Ek alan silinemedi: %1" ).arg( sqlErrorText( q.lastError() ) ), error );
                }
                if ( q.numRowsAffected() != 1 )
                {
                        return fail( QStringLiteral( "Ek alan bulunamadı: %1" ).arg( key ), error );
                }

                emit changed();
                return true;
        }


        ///
        /// Load members
        ///
        bool MemberDatabase::members( QList<Member>& list, bool includePassive, QString* error )
        {
                list.clear();

                QList<MemberField> custom;
                if ( !customFields( custom, error ) )
                {
                        return false;
                }

                QSqlDatabase db = QSqlDatabase::database( mConnectionName, false );

                QStringList cols;
                for ( const auto& f : standardFields() )
                {
                        cols << columnFor( f.key );
                }

                QString sql = QStringLiteral( "SELECT id, active, %1 FROM members %2 ORDER BY %3 COLLATE NOCASE, %4 COLLATE NOCASE, %5 COLLATE NOCASE, id" )
                        .arg( cols.join( ", " ),
                              includePassive ? QString() : QStringLiteral( "WHERE active = 1" ),
                              columnFor( "FIRMA" ), columnFor( "AD" ), columnFor( "SOYAD" ) );

                QSqlQuery q( db );
                q.setForwardOnly( true );
                if ( !q.exec( sql ) )
                {
                        return fail( QStringLiteral( "Üyeler okunamadı: %1" ).arg( sqlErrorText( q.lastError() ) ), error );
                }

                QMap<qint64,int> indexById;
                while ( q.next() )
                {
                        Member m;
                        m.id = q.value( 0 ).toLongLong();
                        m.active = q.value( 1 ).toInt() != 0;
                        int i = 2;
                        for ( const auto& f : standardFields() )
                        {
                                m.values[f.key] = q.value( i++ ).toString();
                        }
                        for ( const auto& f : custom )
                        {
                                m.values[f.key] = QString();
                        }
                        indexById[m.id] = list.size();
                        list.append( m );
                }

                // SQLite's NOCASE only knows ASCII: re-sort with Turkish rules (Ç after C, İ after I, …)
                {
                        QCollator collator{ QLocale( QLocale::Turkish ) };
                        collator.setCaseSensitivity( Qt::CaseInsensitive );
                        collator.setNumericMode( true );
                        std::stable_sort( list.begin(), list.end(), [&]( const Member& a, const Member& b ) {
                                for ( const char* key : { "FIRMA", "AD", "SOYAD" } )
                                {
                                        int c = collator.compare( a.values.value( key ), b.values.value( key ) );
                                        if ( c != 0 )
                                        {
                                                return c < 0;
                                        }
                                }
                                return false;
                        } );
                        indexById.clear();
                        for ( int i = 0; i < list.size(); i++ )
                        {
                                indexById[list[i].id] = i;
                        }
                }

                if ( !custom.isEmpty() && !list.isEmpty() )
                {
                        QSqlQuery v( db );
                        v.setForwardOnly( true );
                        if ( !v.exec( QStringLiteral( "SELECT member_id, field_key, value FROM member_values" ) ) )
                        {
                                list.clear();
                                return fail( QStringLiteral( "Ek alan değerleri okunamadı: %1" ).arg( sqlErrorText( v.lastError() ) ), error );
                        }
                        while ( v.next() )
                        {
                                auto it = indexById.find( v.value( 0 ).toLongLong() );
                                if ( it != indexById.end() )
                                {
                                        list[it.value()].values[v.value( 1 ).toString()] = v.value( 2 ).toString();
                                }
                        }
                }

                return true;
        }


        ///
        /// Insert or update a member (member.id is set after insert)
        ///
        bool MemberDatabase::saveMember( Member& member, QString* error )
        {
                QList<MemberField> custom;
                if ( !customFields( custom, error ) )
                {
                        return false;
                }

                QSqlDatabase db = QSqlDatabase::database( mConnectionName, false );
                if ( !db.transaction() )
                {
                        return fail( QStringLiteral( "İşlem başlatılamadı: %1" ).arg( sqlErrorText( db.lastError() ) ), error );
                }

                auto abort = [&]( const QString& msg ) {
                        db.rollback();
                        return fail( msg, error );
                };

                QStringList cols;
                for ( const auto& f : standardFields() )
                {
                        cols << columnFor( f.key );
                }

                QSqlQuery q( db );
                if ( member.id == 0 )
                {
                        QStringList marks;
                        for ( int i = 0; i < cols.size(); i++ )
                        {
                                marks << QStringLiteral( "?" );
                        }
                        q.prepare( QStringLiteral( "INSERT INTO members (active, %1) VALUES (?, %2)" )
                                   .arg( cols.join( ", " ), marks.join( ", " ) ) );
                }
                else
                {
                        QStringList sets;
                        for ( const auto& c : cols )
                        {
                                sets << QStringLiteral( "%1 = ?" ).arg( c );
                        }
                        q.prepare( QStringLiteral( "UPDATE members SET active = ?, %1, updated_at = datetime('now') WHERE id = ?" )
                                   .arg( sets.join( ", " ) ) );
                }

                q.addBindValue( member.active ? 1 : 0 );
                for ( const auto& f : standardFields() )
                {
                        q.addBindValue( textValue( member.values.value( f.key ) ) );
                }
                if ( member.id != 0 )
                {
                        q.addBindValue( member.id );
                }

                if ( !q.exec() )
                {
                        return abort( QStringLiteral( "Üye kaydedilemedi: %1" ).arg( sqlErrorText( q.lastError() ) ) );
                }

                qint64 id = member.id;
                if ( id == 0 )
                {
                        QVariant newId = q.lastInsertId();
                        if ( !newId.isValid() )
                        {
                                return abort( QStringLiteral( "Yeni üyenin numarası alınamadı." ) );
                        }
                        id = newId.toLongLong();
                }
                else if ( q.numRowsAffected() != 1 )
                {
                        return abort( QStringLiteral( "Üye bulunamadı (silinmiş olabilir)." ) );
                }

                QSqlQuery del( db );
                del.prepare( QStringLiteral( "DELETE FROM member_values WHERE member_id = ?" ) );
                del.addBindValue( id );
                if ( !del.exec() )
                {
                        return abort( QStringLiteral( "Ek alan değerleri güncellenemedi: %1" ).arg( sqlErrorText( del.lastError() ) ) );
                }

                for ( const auto& f : custom )
                {
                        QString value = member.values.value( f.key ).trimmed();
                        if ( value.isEmpty() )
                        {
                                continue;
                        }
                        QSqlQuery ins( db );
                        ins.prepare( QStringLiteral( "INSERT INTO member_values (member_id, field_key, value) VALUES (?, ?, ?)" ) );
                        ins.addBindValue( id );
                        ins.addBindValue( f.key );
                        ins.addBindValue( textValue( value ) );
                        if ( !ins.exec() )
                        {
                                return abort( QStringLiteral( "Ek alan değeri kaydedilemedi: %1" ).arg( sqlErrorText( ins.lastError() ) ) );
                        }
                }

                if ( !db.commit() )
                {
                        return abort( QStringLiteral( "Kayıt tamamlanamadı: %1" ).arg( sqlErrorText( db.lastError() ) ) );
                }

                member.id = id;
                emit changed();
                return true;
        }


        bool MemberDatabase::deleteMembers( const QList<qint64>& ids, QString* error )
        {
                if ( ids.isEmpty() )
                {
                        return true;
                }
                if ( !ensureOpen( error ) )
                {
                        return false;
                }

                QSqlDatabase db = QSqlDatabase::database( mConnectionName, false );
                if ( !db.transaction() )
                {
                        return fail( QStringLiteral( "İşlem başlatılamadı: %1" ).arg( sqlErrorText( db.lastError() ) ), error );
                }
                for ( qint64 id : ids )
                {
                        QSqlQuery q( db );
                        q.prepare( QStringLiteral( "DELETE FROM members WHERE id = ?" ) );
                        q.addBindValue( id );
                        if ( !q.exec() )
                        {
                                QString msg = sqlErrorText( q.lastError() );
                                db.rollback();
                                return fail( QStringLiteral( "Üye silinemedi: %1" ).arg( msg ), error );
                        }
                }
                if ( !db.commit() )
                {
                        QString msg = sqlErrorText( db.lastError() );
                        db.rollback();
                        return fail( QStringLiteral( "Silme tamamlanamadı: %1" ).arg( msg ), error );
                }

                emit changed();
                return true;
        }


        bool MemberDatabase::setActive( const QList<qint64>& ids, bool active, QString* error )
        {
                if ( ids.isEmpty() )
                {
                        return true;
                }
                if ( !ensureOpen( error ) )
                {
                        return false;
                }

                QSqlDatabase db = QSqlDatabase::database( mConnectionName, false );
                if ( !db.transaction() )
                {
                        return fail( QStringLiteral( "İşlem başlatılamadı: %1" ).arg( sqlErrorText( db.lastError() ) ), error );
                }
                for ( qint64 id : ids )
                {
                        QSqlQuery q( db );
                        q.prepare( QStringLiteral( "UPDATE members SET active = ?, updated_at = datetime('now') WHERE id = ?" ) );
                        q.addBindValue( active ? 1 : 0 );
                        q.addBindValue( id );
                        if ( !q.exec() )
                        {
                                QString msg = sqlErrorText( q.lastError() );
                                db.rollback();
                                return fail( QStringLiteral( "Üye durumu değiştirilemedi: %1" ).arg( msg ), error );
                        }
                }
                if ( !db.commit() )
                {
                        QString msg = sqlErrorText( db.lastError() );
                        db.rollback();
                        return fail( QStringLiteral( "Durum değişikliği tamamlanamadı: %1" ).arg( msg ), error );
                }

                emit changed();
                return true;
        }

}
