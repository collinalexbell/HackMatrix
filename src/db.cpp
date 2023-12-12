#include <fstream>
#include <iostream>
#include <pqxx/connection.hxx>
#include <pqxx/pqxx>
#include <sstream>

int main()
{
  try
  {
    // Connect to the database.  In practice we may have to pass some
    // arguments to say where the database server is, and so on.
    // The constructor parses options exactly like libpq's
    // PQconnectdb/PQconnect, see:
    // https://www.postgresql.org/docs/10/static/libpq-connect.html

    std::ifstream ifs("secrets/db-pass");
    std::string password((std::istreambuf_iterator<char>(ifs)),
                        (std::istreambuf_iterator<char>()));

    std::stringstream connectionParams;
    connectionParams
        << "postgresql://localhost/matrix?user=collinalexbell&password="
        << password;
    pqxx::connection c(connectionParams.str());
    // Start a transaction.  In libpqxx, you always work in one.
    pqxx::work w(c);
    // work::exec1() executes a query returning a single row of data.
    // We'll just ask the database to return the number 1 to us.
    pqxx::row r = w.exec1("SELECT 1");
    // Commit your transaction.  If an exception occurred before this
    // point, execution will have left the block, and the transaction will
    // have been destroyed along the way.  In that case, the failed
    // transaction would implicitly abort instead of getting to this point.
    w.commit();
    // Look at the first and only field in the row, parse it as an integer,
    // and print it.
    //
    // "r[0]" returns the first field, which has an "as<...>()" member
    // function template to convert its contents from their string format
    // to a type of your choice.
    std::cout << r[0].as<int>() << std::endl;
  }
  catch (std::exception const &e)
  {
    std::cerr << e.what() << std::endl;
    return 1;
  }
}
