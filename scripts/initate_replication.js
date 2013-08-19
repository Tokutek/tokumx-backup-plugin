rsconf = { _id: "myreplsetid", members: [ {_id:0, host:"localhost:11223" } ] };
rs.initiate( rsconf );
rs.conf();
rs.add("localhost:11224");
