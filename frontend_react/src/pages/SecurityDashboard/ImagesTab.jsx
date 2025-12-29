import React from 'react';
import ImageGallery from '../../components/Images/ImageGallery';
import ImageModal from '../../components/Images/ImageModal';

function ImagesTab({ images, isLoading, selectedImage, onImageClick, onCloseModal }) {
  return (
    <>
      <ImageGallery images={images} isLoading={isLoading} onImageClick={onImageClick} />
      <ImageModal image={selectedImage} onClose={onCloseModal} />
    </>
  );
}

export default ImagesTab;
