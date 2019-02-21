target:
	chmod u+x createVersionHeader.sh
	./createVersionHeader.sh
	platformio ci --project-conf=platformio.ini src

clean:
	platformio run --target clean

upload:
	platformio run --target upload

manual:
	chmod u+x doc/generateDocumentationAndDeploy.sh
	./doc/generateDocumentationAndDeploy.sh
