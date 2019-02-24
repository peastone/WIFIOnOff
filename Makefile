target:
	./createVersionHeader.sh
	platformio ci --project-conf=platformio.ini src

clean:
	platformio run --target clean

upload:
	platformio run --target upload

manual:
	./doc/generateDocumentationAndDeploy.sh
