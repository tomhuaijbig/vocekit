#include <QCoreApplication>
#include <QSslSocket>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream output(stdout);

    output << "Qt SSL build version: " << QSslSocket::sslLibraryBuildVersionString() << '\n';
    output << "Qt SSL runtime version: " << QSslSocket::sslLibraryVersionString() << '\n';
    output << "SSL supported: " << (QSslSocket::supportsSsl() ? "yes" : "no") << '\n';
    output.flush();

    return QSslSocket::supportsSsl() ? 0 : 1;
}
