// add_data.js -
// Adds data for backup tests.

db = db.getSiblingDB('<db>');
var i = 0;
while (i < 100)
{
    // If we use a js variable, and insert that into a db, it is not
    // permanently added to the collection!?!?  UG!
    dummy = { id : i };
    db.backupTestData.insert(dummy);
    i++;
}
