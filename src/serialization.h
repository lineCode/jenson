/****************************************************************************

 Copyright (c) 2014, Hans Robeers
 All rights reserved.

 BSD 2-Clause License

 Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

   * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

   * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

****************************************************************************/

#ifndef JENSON_SERIALIZATION_H
#define JENSON_SERIALIZATION_H

#define SERIALIZABLE(CLASS, SERIAL_NAME) Q_DECLARE_METATYPE(CLASS *) \
    namespace serialization_register { /*Avoid name clashes with global variables*/\
        static jenson::serialization::registerForSerialization<CLASS> SERIAL_NAME(#SERIAL_NAME);\
    }

#define CUSTOMSERIALIZABLE(CLASS, CUSTOM_SERIALIZER_CLASS, SERIAL_NAME) Q_DECLARE_METATYPE(CLASS *) \
    namespace serialization_register { /*Avoid name clashes with global variables*/\
        static const CUSTOM_SERIALIZER_CLASS SERIAL_NAME##_SERIALIZER; \
        static jenson::serialization::registerForSerialization<CLASS> SERIAL_NAME(#SERIAL_NAME, &SERIAL_NAME##_SERIALIZER);\
    }


#include <QObject>
#include <QJsonObject>
#include <QMetaProperty>
#include "boost/bimap.hpp"

namespace jenson
{
    typedef boost::bimap<QString, QString> nm_type;

    class SerializationException : public std::exception
    {
    private:
        QString _message;

    public:
        explicit SerializationException(QString &message) throw()
            : _message(message) {}

        virtual const char* what() const throw() override { return _message.toStdString().c_str(); }

        virtual ~SerializationException() throw() {}
    };

    class serialization
    {
    public:
        //
        // Classes for custom serialization
        //

        class ICustomSerializer
        {
        public:
            virtual QJsonValue serialize(const QObject *object) const = 0;
            virtual std::unique_ptr<QObject> deserialize(const QJsonValue *jsonValue, QString *errorMsg = 0) const = 0;

            virtual ~ICustomSerializer() {}
        };

        template <typename T>
        class CustomSerializer : public ICustomSerializer
        {
        protected:
            virtual QJsonValue serializeImpl(const T *object) const = 0;
            virtual std::unique_ptr<T> deserializeImpl(const QJsonValue *jsonValue, QString *errorMsg) const = 0;

        public:
            virtual QJsonValue serialize(const QObject *object) const override final
                { return serializeImpl(qobject_cast<const T*>(object)); }
            virtual std::unique_ptr<QObject> deserialize(const QJsonValue *jsonValue, QString *errorMsg = 0) const override final
                { return deserializeImpl(jsonValue, errorMsg); }

            virtual ~CustomSerializer() {}
        };

    private:
        static QMap<QString, const QObject*>& typeMapPriv()
        {
            static QMap<QString, const QObject*> tMap;
            return tMap;
        }
        static QMap<QString, const ICustomSerializer*>& serializerMapPriv()
        {
            static QMap<QString, const ICustomSerializer*> sMap;
            return sMap;
        }
        static nm_type& nameMapPriv()
        {
            static nm_type  nMap;
            return nMap;
        }

    public:
        // Exception throwing methods
        static QJsonObject serialize(const QObject *qObj);
        static std::unique_ptr<QObject> deserializeToObject(const QJsonObject *jsonObj);
        static std::unique_ptr<QObject> deserializeClass(const QJsonObject *jsonObj, QString className);

        // ErrorMsg methods
        static std::unique_ptr<QObject> deserializeToObject(const QJsonObject *jsonObj, QString *errorMsg);
        static std::unique_ptr<QObject> deserializeClass(const QJsonObject *jsonObj, QString className, QString *errorMsg);

        // Casting methods
        template <typename T>
        static std::unique_ptr<T> deserialize(const QJsonObject *jsonObj, QString *errorMsg)
        {
            QObject* deserialized = deserializeToObject(jsonObj, errorMsg).release();
            T* casted = qobject_cast<T*>(deserialized);
            if (!casted)
            {
                delete deserialized;
                T instance;
                if (errorMsg)
                {
                    errorMsg->append("\n Failed to cast to type: ");
                    errorMsg->append(instance.metaObject()->className());
                }
            }
            return std::unique_ptr<T>(casted);
        }
        template <typename T>
        static std::unique_ptr<T> deserialize(const QJsonObject *jsonObj)
        {
            QString errorMsg;
            std::unique_ptr<T> retVal = deserialize<T>(jsonObj, &errorMsg);
            if (!retVal) throw SerializationException(errorMsg);
            return retVal;
        }

        // Public map getters
        static const QMap<QString, const QObject*>& typeMap() { return typeMapPriv(); }
        static const QMap<QString, const ICustomSerializer*>& serializerMap() { return serializerMapPriv(); }
        static const nm_type& nameMap() { return nameMapPriv(); }

        // Auxilliary methods
        static bool isRegistered(QString *className, QString *errorMsg = 0);
        static QString toSerialName(QString className);
        static QString toClassName(QString serialName);

        // Registration class (Use SERIALIZABLE macro)
        template <typename T>
        class registerForSerialization
        {
        public:
            registerForSerialization(QString serialName, const ICustomSerializer* serializer = nullptr)
            {
                static const T t;
                typeMapPriv()[t.metaObject()->className()] = &t;
                nameMapPriv().insert(nm_type::value_type(t.metaObject()->className(), serialName));
                if (serializer) serializerMapPriv()[t.metaObject()->className()] = serializer;
                qRegisterMetaType<T*>();
            }
        };
    };

    template <typename T>
    static QList<const T*> toConstList(QList<std::shared_ptr<T>> list)
    {
        QList<const T *> retVal;
        for (std::shared_ptr<T> &item: list) {
            retVal.append(item.get());
        }
        return retVal;
    }
}

#endif // JENSON_SERIALIZATION_H