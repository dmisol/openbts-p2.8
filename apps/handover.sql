INSERT INTO "CONFIG" VALUES('GSM.Handover.T3105','50000','0','0','interval (in usec) between sending PhysicalInformation');
INSERT INTO "CONFIG" VALUES('GSM.Handover.T3103','3000','0','0','Handover attempt timeout');
INSERT INTO "CONFIG" VALUES('GSM.Handover.Ny1','20','0','0','Number of attempts to deliver Physical Information');

INSERT INTO "CONFIG" VALUES('GSM.Handover.BTS.NeighborsFilename','/etc/OpenBTS/Neighbors.txt',0,0,'Handover Decision at BTS: ARFCN and addresses of neighbor sites');
INSERT INTO "CONFIG" VALUES('GSM.Handover.BTS.Enable','1',0,0,'if not 0, Handover Decision can be taken locally');
INSERT INTO "CONFIG" VALUES('GSM.Handover.BTS.Hysteresis','10',0,0,'difference (dB) between serving cell and candidate for handover');
INSERT INTO "CONFIG" VALUES('GSM.Handover.BTS.Weights','0.7',0,0,'a value between 0 and 1, the weight of the current sample when averaging measurement results');
INSERT INTO "CONFIG" VALUES('GSM.Handover.BTS.Interleave','10',0,0,'Handover Decision is tested at every N-th measurement to reduce load');

INSERT INTO "CONFIG" VALUES('GSM.Handover.Debug.NeighbourIp','192.168.1.9:5062','0','0','Temporary field, just a way to do without hardcoding during evaluating');
